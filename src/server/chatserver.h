#pragma once

#include <QObject>
#include <QTcpServer>
#include <QList>
#include <QHash>
#include <QPointer>
#include <QJsonArray>

class ClientHandler;
class DatabaseWorker;
class QThread;

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer();
    bool start(quint16 port = 8888);

private slots:
    void onNewConnection();
    void onLoginRequest(const QString &username);
    void onChatMessage(const QString &to, const QString &content);
    void onHistoryRequest(const QString &keyword);
    void onAckRequest(const QString &fromUser);
    void onClientDisconnected();
    void onMessageSaved(int msgId);
    void onHistoryResult(int requestId, const QJsonArray &messages);

private:
    ClientHandler *findHandler(const QString &username) const;

    QTcpServer *m_server;
    QList<ClientHandler *> m_clients;
    QHash<QString, ClientHandler *> m_users;

    DatabaseWorker *m_dbWorker;
    QThread *m_dbThread;

    int m_msgCounter = 0;
    QHash<int, QPair<QString, QString>> m_pendingMsgIds;  // msgId -> (from, to)

    int m_nextHistoryRequestId = 1;
    QHash<int, QPointer<ClientHandler>> m_pendingHistoryReqs;  // requestId -> handler
};
