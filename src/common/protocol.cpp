#include "protocol.h"
#include <QDataStream>
#include <QHash>

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
    QByteArray raw = socket->readAll();

    // 追加到内部缓冲区
    QByteArray &buffer = s_buffers[socket];
    buffer.append(raw);

    // 解析长度头 + JSON
    while (buffer.size() >= 4) {
        quint32 len;
        QDataStream stream(buffer.left(4));
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> len;

        if ((quint32)buffer.size() < 4 + len)
            break; // 数据未到齐

        QByteArray jsonData = buffer.mid(4, len);
        buffer.remove(0, 4 + len);

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            messages.append(doc.object());
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

QJsonObject Protocol::buildChat(const QString &to, const QString &content)
{
    QJsonObject json;
    json["type"] = CHAT;
    json["to"] = to;
    json["content"] = content;
    return json;
}

QJsonObject Protocol::buildHistory(const QString &keyword)
{
    QJsonObject json;
    json["type"] = HISTORY;
    json["keyword"] = keyword;
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

QJsonObject Protocol::buildChatMessage(const QString &from, const QString &content)
{
    QJsonObject json;
    json["type"] = CHAT;
    json["from"] = from;
    json["content"] = content;
    return json;
}

QJsonObject Protocol::buildHistoryResult(const QJsonArray &messages)
{
    QJsonObject json;
    json["type"] = HISTORY_RESULT;
    json["messages"] = messages;
    return json;
}
