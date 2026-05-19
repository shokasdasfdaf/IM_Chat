#include "chatserver.h"
#include "clienthandler.h"
#include "protocol.h"
#include <QDebug>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ChatServer::onNewConnection);
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

    // 广播新用户加入
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
    }
}

void ChatServer::onClientDisconnected()
{
    auto *handler = qobject_cast<ClientHandler *>(sender());
    if (!handler)
        return;

    m_clients.removeOne(handler);

    if (!handler->username().isEmpty()) {
        m_users.remove(handler->username());

        // 广播用户离开
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

void ChatServer::broadcastUserList()
{
    QStringList users = m_users.keys();
    for (auto *c : m_clients) {
        if (!c->username().isEmpty())
            c->sendJson(Protocol::buildLoginOk(users));
    }
}

ClientHandler *ChatServer::findHandler(const QString &username) const
{
    return m_users.value(username, nullptr);
}
