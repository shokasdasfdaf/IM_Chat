#include "chatclient.h"
#include "protocol.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(10000);
    connect(m_heartbeatTimer, &QTimer::timeout,
            this, &ChatClient::onHeartbeat);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &ChatClient::onReconnect);

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,
            this, &ChatClient::doLogin);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &ChatClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ChatClient::onDisconnected);
}

void ChatClient::connectToServer(const QString &host, quint16 port)
{
    m_serverHost = host;
    m_serverPort = port;
    m_socket->connectToHost(host, port);
}

bool ChatClient::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatClient::login(const QString &username)
{
    m_pendingLogin = username;
    m_lastUsername = username;
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        doLogin();
    }
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

void ChatClient::sendAck(const QString &fromUser)
{
    Protocol::sendMessage(m_socket, Protocol::buildAck(fromUser));
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
            m_heartbeatTimer->start();
            m_reconnectTimer->stop();
            m_reconnecting = false;
            m_missedPongs = 0;
            if (m_reconnectCount > 0) {
                m_reconnectCount = 0;
                emit reconnected();
            }
            emit loginSuccess(users);
        } else if (type == Protocol::LOGIN_FAILED) {
            emit loginFailed(msg["reason"].toString());
        } else if (type == Protocol::USER_JOINED) {
            emit userJoined(msg["username"].toString());
        } else if (type == Protocol::USER_LEFT) {
            emit userLeft(msg["username"].toString());
        } else if (type == Protocol::CHAT) {
            int msgId = msg.value("msg_id").toInt(0);
            emit messageReceived(msg["from"].toString(),
                                 msg["content"].toString(), msgId);
        } else if (type == Protocol::HISTORY_RESULT) {
            emit historyReceived(msg["messages"].toArray());
        } else if (type == Protocol::PONG) {
            m_missedPongs = 0;
        }
    }
}

void ChatClient::onDisconnected()
{
    qDebug() << "!!! DISCONNECTED !!!";
    m_heartbeatTimer->stop();

    if (m_reconnecting)
        return;

    if (!m_lastUsername.isEmpty() && !m_serverHost.isEmpty()) {
        m_reconnecting = true;
        m_reconnectTimer->start();
        emit reconnecting();
    } else {
        emit connectionError("已断开连接");
    }
}

void ChatClient::onHeartbeat()
{
    m_missedPongs++;
    if (m_missedPongs >= 3) {
        m_heartbeatTimer->stop();
        m_socket->disconnectFromHost();
        return;
    }
    QJsonObject ping;
    ping["type"] = Protocol::PING;
    Protocol::sendMessage(m_socket, ping);
}

void ChatClient::onReconnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectingState
        || m_socket->state() == QAbstractSocket::ConnectedState)
        return;

    // 最大重试次数，避免无限重连风暴
    constexpr int MAX_RECONNECT = 10;
    if (m_reconnectCount >= MAX_RECONNECT) {
        m_reconnectTimer->stop();
        m_reconnecting = false;
        m_reconnectCount = 0;
        emit connectionError("重连失败，已达最大重试次数");
        return;
    }

    qDebug() << "Reconnecting..." << (m_reconnectCount + 1) << "/" << MAX_RECONNECT;
    m_reconnectCount++;

    // m_reconnecting 保持 true，直到 LOGIN_OK 收到才解除
    // 不要在 abort() 之后立刻置 false，因为 disconnected 是 queued 信号
    m_reconnecting = true;
    Protocol::resetBuffer(m_socket);
    m_socket->abort();

    m_pendingLogin = m_lastUsername;
    m_socket->connectToHost(m_serverHost, m_serverPort);
}
