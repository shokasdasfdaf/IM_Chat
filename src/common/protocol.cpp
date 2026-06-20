#include "protocol.h"
#include <QDataStream>
#include <QHash>
#include <QDebug>

namespace {
    thread_local QHash<QTcpSocket *, QByteArray> s_buffers;
}

// 4 字节大端长度头 + JSON
QByteArray Protocol::pack(const QJsonObject &json)
{
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << (quint32)data.size();
    packet.append(data);
    return packet;
}

void Protocol::sendMessage(QTcpSocket *socket, const QJsonObject &json)
{
    socket->write(pack(json));
}

void Protocol::sendMessage(QTcpSocket *socket, const QByteArray &data)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << (quint32)data.size();
    packet.append(data);
    socket->write(packet);
}

QList<QJsonObject> Protocol::receiveMessages(QTcpSocket *socket)
{
    QList<QJsonObject> messages;
    if (!socket)
        return messages;

    QByteArray raw = socket->readAll();

    // 首次见到此 socket：挂 destroyed 信号自动清缓冲，防止裸指针被新 socket 复用串台
    if (!s_buffers.contains(socket)) {
        QObject::connect(socket, &QObject::destroyed, [](QObject *obj) {
            s_buffers.remove(static_cast<QTcpSocket *>(obj));
        });
    }
    QByteArray &buffer = s_buffers[socket];
    buffer.append(raw);

    while (buffer.size() >= 4) {
        quint32 len;
        QDataStream stream(buffer.left(4));
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> len;

        // 防御：包长超过上限直接断连，避免 hang 住等数据 / 内存爆掉
        if (len > MAX_PACKET_SIZE) {
            qWarning() << "Protocol: packet length" << len
                       << "exceeds MAX_PACKET_SIZE, aborting socket"
                       << socket->peerAddress().toString();
            buffer.clear();
            socket->abort();
            return messages;
        }

        if ((quint32)buffer.size() < 4 + len)
            break;

        QByteArray jsonData = buffer.mid(4, len);
        buffer.remove(0, 4 + len);

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            messages.append(doc.object());
        } else {
            qWarning() << "Protocol: invalid JSON dropped, error:" << error.errorString();
        }
    }

    return messages;
}

void Protocol::resetBuffer(QTcpSocket *socket)
{
    s_buffers.remove(socket);
}

// ---- 构建消息 ----

QJsonObject Protocol::buildLogin(const QString &username)
{
    QJsonObject json;
    json["type"] = LOGIN;
    json["username"] = username;
    return json;
}

QJsonObject Protocol::buildChat(const QString &to, const QString &content, int msgId)
{
    QJsonObject json;
    json["type"] = CHAT;
    json["to"] = to;
    json["content"] = content;
    if (msgId > 0)
        json["msg_id"] = msgId;
    return json;
}

QJsonObject Protocol::buildHistory(const QString &keyword)
{
    QJsonObject json;
    json["type"] = HISTORY;
    json["keyword"] = keyword;
    return json;
}

QJsonObject Protocol::buildAck(const QString &fromUser)
{
    QJsonObject json;
    json["type"] = ACK;
    json["from"] = fromUser;
    return json;
}

QJsonObject Protocol::buildLoginOk(const QStringList &users)
{
    QJsonObject json;
    json["type"] = LOGIN_OK;
    QJsonArray arr;
    for (const QString &user : users)
        arr.append(user);
    json["users"] = arr;
    return json;
}

QJsonObject Protocol::buildLoginFailed(const QString &reason)
{
    QJsonObject json;
    json["type"] = LOGIN_FAILED;
    json["reason"] = reason;
    return json;
}

QJsonObject Protocol::buildUserJoined(const QString &username)
{
    QJsonObject json;
    json["type"] = USER_JOINED;
    json["username"] = username;
    return json;
}

QJsonObject Protocol::buildUserLeft(const QString &username)
{
    QJsonObject json;
    json["type"] = USER_LEFT;
    json["username"] = username;
    return json;
}

QJsonObject Protocol::buildChatMessage(const QString &from, const QString &content, int msgId)
{
    QJsonObject json;
    json["type"] = CHAT;
    json["from"] = from;
    json["content"] = content;
    if (msgId > 0)
        json["msg_id"] = msgId;
    return json;
}

QJsonObject Protocol::buildHistoryResult(const QJsonArray &messages)
{
    QJsonObject json;
    json["type"] = HISTORY_RESULT;
    json["messages"] = messages;
    return json;
}

QJsonObject Protocol::buildMsgRead(int msgId)
{
    QJsonObject json;
    json["type"] = MSG_READ;
    json["msg_id"] = msgId;
    return json;
}
