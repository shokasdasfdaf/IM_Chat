#include "chatserver.h"
#include "clienthandler.h"
#include "protocol.h"
#include "DatabaseWorker.h"
#include <QThread>
#include <QDebug>
#include <memory>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ChatServer::onNewConnection);

    m_dbWorker = new DatabaseWorker();
    m_dbThread = new QThread(this);
    m_dbWorker->moveToThread(m_dbThread);

    connect(m_dbThread, &QThread::started, m_dbWorker, &DatabaseWorker::initialize);
    connect(m_dbThread, &QThread::finished, m_dbWorker, &QObject::deleteLater);
    connect(m_dbWorker, &DatabaseWorker::messageSaved,
            this, &ChatServer::onMessageSaved);
    connect(m_dbWorker, &DatabaseWorker::historyResult,
            this, &ChatServer::onHistoryResult);
    m_dbThread->start();
}

ChatServer::~ChatServer()
{
    m_dbThread->quit();
    m_dbThread->wait();
}

bool ChatServer::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning() << "Server listen failed:" << m_server->errorString();
        return false;
    }
    qDebug() << "Server listening on port" << port;
    return true;
}

void ChatServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        auto *handler = new ClientHandler(socket, this);

        connect(handler, &ClientHandler::loginRequest,
                this, &ChatServer::onLoginRequest);
        connect(handler, &ClientHandler::chatMessage,
                this, &ChatServer::onChatMessage);
        connect(handler, &ClientHandler::historyRequest,
                this, &ChatServer::onHistoryRequest);
        connect(handler, &ClientHandler::ackRequest,
                this, &ChatServer::onAckRequest);
        connect(handler, &ClientHandler::disconnected,
                this, &ChatServer::onClientDisconnected);

        m_clients.append(handler);
        qDebug() << "New connection from" << socket->peerAddress().toString();
    }
}

void ChatServer::onLoginRequest(const QString &username)
{
    auto *handler = qobject_cast<ClientHandler *>(sender());
    if (!handler)
        return;

    if (m_users.contains(username)) {
        auto *oldHandler = m_users[username];
        m_clients.removeOne(oldHandler);
        m_users.remove(username);
        oldHandler->setUsername(QString());
        oldHandler->deleteLater();
        qDebug() << "Kicked old connection for:" << username;
    }

    handler->setUsername(username);
    m_users[username] = handler;

    handler->sendJson(Protocol::buildLoginOk(m_users.keys()));

    QJsonObject joined = QJsonObject{
        {"type", Protocol::USER_JOINED},
        {"username", username}
    };
    for (auto *c : m_clients) {
        if (c != handler && !c->username().isEmpty())
            c->sendJson(joined);
    }

    qDebug() << "User logged in:" << username;
}

void ChatServer::onChatMessage(const QString &to, const QString &content)
{
    auto *from = qobject_cast<ClientHandler *>(sender());
    if (!from)
        return;

    m_msgCounter++;
    int msgId = m_msgCounter;
    m_pendingMsgIds[msgId] = {from->username(), to};

    auto *target = findHandler(to);
    if (target) {
        target->sendJson(Protocol::buildChatMessage(from->username(), content, msgId));
        qDebug() << "Chat:" << from->username() << "->" << to << ":" << content << "id:" << msgId;
    }

    QMetaObject::invokeMethod(m_dbWorker, "saveMessage", Qt::QueuedConnection,
                              Q_ARG(QString, from->username()),
                              Q_ARG(QString, to),
                              Q_ARG(QString, content));
}

void ChatServer::onHistoryRequest(const QString &keyword)
{
    auto *handler = qobject_cast<ClientHandler *>(sender());
    if (!handler)
        return;

    // 用 requestId 路由结果，避免多客户端并发请求时 lambda 抢占 emit 错位
    int requestId = m_nextHistoryRequestId++;
    m_pendingHistoryReqs.insert(requestId, handler);

    QMetaObject::invokeMethod(m_dbWorker, "searchHistory", Qt::QueuedConnection,
                              Q_ARG(int, requestId),
                              Q_ARG(QString, keyword));
}

void ChatServer::onHistoryResult(int requestId, const QJsonArray &messages)
{
    auto it = m_pendingHistoryReqs.find(requestId);
    if (it == m_pendingHistoryReqs.end())
        return;
    QPointer<ClientHandler> handler = it.value();
    m_pendingHistoryReqs.erase(it);

    // QPointer 自动判空：handler 在结果返回前已断开/析构则丢弃
    if (handler)
        handler->sendJson(Protocol::buildHistoryResult(messages));
}

void ChatServer::onAckRequest(const QString &fromUser)
{
    auto *handler = qobject_cast<ClientHandler *>(sender());
    if (!handler)
        return;

    QMetaObject::invokeMethod(m_dbWorker, "markRead", Qt::QueuedConnection,
                              Q_ARG(QString, fromUser),
                              Q_ARG(QString, handler->username()));
}

void ChatServer::onClientDisconnected()
{
    auto *handler = qobject_cast<ClientHandler *>(sender());
    if (!handler)
        return;

    m_clients.removeOne(handler);

    if (!handler->username().isEmpty()) {
        m_users.remove(handler->username());

        QJsonObject left = QJsonObject{
            {"type", Protocol::USER_LEFT},
            {"username", handler->username()}
        };
        for (auto *c : m_clients) {
            if (!c->username().isEmpty())
                c->sendJson(left);
        }

        qDebug() << "User left:" << handler->username();
    }

    handler->deleteLater();
}

ClientHandler *ChatServer::findHandler(const QString &username) const
{
    return m_users.value(username, nullptr);
}

void ChatServer::onMessageSaved(int msgId)
{
    if (msgId < 0 || !m_pendingMsgIds.contains(msgId))
        return;
    m_pendingMsgIds.remove(msgId);
}
