#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QTcpSocket>

class Protocol {
public:
    // 单帧最大长度，防止恶意 client 发送 0xFFFFFFFF 让对端 hang 住等数据
    static constexpr quint32 MAX_PACKET_SIZE = 1 * 1024 * 1024; // 1MB

    // ---- 消息类型常量 ----
    // 客户端 → 服务器
    static constexpr const char *LOGIN   = "login";
    static constexpr const char *CHAT    = "chat";
    static constexpr const char *HISTORY = "history";
    static constexpr const char *PING    = "ping";
    static constexpr const char *ACK     = "ack";

    // 服务器 → 客户端
    static constexpr const char *PONG          = "pong";
    static constexpr const char *LOGIN_OK      = "login_ok";
    static constexpr const char *LOGIN_FAILED  = "login_failed";
    static constexpr const char *USER_JOINED   = "user_joined";
    static constexpr const char *USER_LEFT     = "user_left";
    static constexpr const char *HISTORY_RESULT = "history_result";
    static constexpr const char *MSG_READ      = "msg_read";

    // ---- 构建消息（返回 JSON 对象，由调用方决定何时打包）----
    static QJsonObject buildLogin(const QString &username);
    static QJsonObject buildChat(const QString &to, const QString &content, int msgId = 0);
    static QJsonObject buildHistory(const QString &keyword);
    static QJsonObject buildAck(const QString &fromUser);

    static QJsonObject buildLoginOk(const QStringList &users);
    static QJsonObject buildLoginFailed(const QString &reason);
    static QJsonObject buildUserJoined(const QString &username);
    static QJsonObject buildUserLeft(const QString &username);
    static QJsonObject buildChatMessage(const QString &from, const QString &content, int msgId = 0);
    static QJsonObject buildHistoryResult(const QJsonArray &messages);
    static QJsonObject buildMsgRead(int msgId);

    // ---- 发送消息（带长度头）----
    static void sendMessage(QTcpSocket *socket, const QJsonObject &json);
    static void sendMessage(QTcpSocket *socket, const QByteArray &data);

    // ---- 接收消息（处理粘包）----
    static QList<QJsonObject> receiveMessages(QTcpSocket *socket);

    // 清空 socket 对应的缓冲区（重连时调用，避免旧数据污染）
    static void resetBuffer(QTcpSocket *socket);

private:
    static QByteArray pack(const QJsonObject &json);
};
