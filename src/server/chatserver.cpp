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

    // 数据库工人在子线程干活
    m_dbWorker = new DatabaseWorker();
    m_dbThread = new QThread(this);
    m_dbWorker->moveToThread(m_dbThread);

    connect(m_dbThread, &QThread::started, m_dbWorker, &DatabaseWorker::initialize);
    connect(m_dbThread, &QThread::finished, m_dbWorker, &QObject::deleteLater);
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
        handler->sendJson(QJsonObject{
            {"type", Protocol::LOGIN_FAILED},
            {"reason", "用户名已在线"}
        });
        return;
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

    auto *target = findHandler(to);
    if (target) {
        target->sendJson(Protocol::buildChatMessage(from->username(), content));
        qDebug() << "Chat:" << from->username() << "->" << to << ":" << content;
    }

    // 异步存数据库（不会堵主线程）
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

    // 用 lambda 连接，搜完就发回给请求者，然后断开
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_dbWorker, &DatabaseWorker::historyResult,
                    handler, [handler, conn](const QJsonArray &messages) {
        handler->sendJson(Protocol::buildHistoryResult(messages));
        QObject::disconnect(*conn);
    });

    QMetaObject::invokeMethod(m_dbWorker, "searchHistory", Qt::QueuedConnection,
                              Q_ARG(QString, keyword));
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
