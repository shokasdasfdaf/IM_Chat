#include "chatclient.h"
#include "protocol.h"
#include <QJsonArray>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected,
            this, &ChatClient::doLogin);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ChatClient::onDisconnected);
}

void ChatClient::connectToServer(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

bool ChatClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::login(const QString &username)
{
    m_pendingLogin = username;
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        doLogin();
    }
    // 否则等 connected 信号触发 doLogin
}

void ChatClient::doLogin()
{
    if (m_pendingLogin.isEmpty())
        return;
    Protocol::sendMessage(m_socket, Protocol::buildLogin(m_pendingLogin));
    m_pendingLogin.clear();
}

void ChatClient::sendMessage(const QString &to, const QString &content)
{
    Protocol::sendMessage(m_socket, Protocol::buildChat(to, content));
}

void ChatClient::requestHistory(const QString &keyword)
{
    Protocol::sendMessage(m_socket, Protocol::buildHistory(keyword));
}

void ChatClient::onReadyRead()
{
    QList<QJsonObject> messages = Protocol::receiveMessages(m_socket);
    for (const QJsonObject &msg : messages) {
        QString type = msg["type"].toString();

        if (type == Protocol::LOGIN_OK) {
            QStringList users;
            for (const QJsonValue &v : msg["users"].toArray())
                users.append(v.toString());
            emit loginSuccess(users);
        } else if (type == Protocol::LOGIN_FAILED) {
            emit loginFailed(msg["reason"].toString());
        } else if (type == Protocol::USER_JOINED) {
            emit userJoined(msg["username"].toString());
        } else if (type == Protocol::USER_LEFT) {
            emit userLeft(msg["username"].toString());
        } else if (type == Protocol::CHAT) {
            emit messageReceived(msg["from"].toString(),
                                 msg["content"].toString());
        } else if (type == Protocol::HISTORY_RESULT) {
            emit historyReceived(msg["messages"].toArray());
        }
    }
}

void ChatClient::onDisconnected()
{
    emit connectionError("已断开连接");
}
