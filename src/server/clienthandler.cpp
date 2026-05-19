#include "clienthandler.h"
#include "protocol.h"
#include <QJsonObject>

ClientHandler::ClientHandler(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket)
{
    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ClientHandler::onDisconnected);
}

void ClientHandler::sendJson(const QJsonObject &json)
{
    Protocol::sendMessage(m_socket, json);
}

void ClientHandler::onReadyRead()
{
    QList<QJsonObject> messages = Protocol::receiveMessages(m_socket);
    for (const QJsonObject &msg : messages) {
        QString type = msg["type"].toString();

        if (type == Protocol::LOGIN) {
            emit loginRequest(msg["username"].toString());
        } else if (type == Protocol::CHAT) {
            emit chatMessage(msg["to"].toString(), msg["content"].toString());
        }
    }
}

void ClientHandler::onDisconnected()
{
    emit disconnected();
}
