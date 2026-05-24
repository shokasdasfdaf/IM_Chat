#pragma once

#include <QObject>
#include <QTcpServer>
#include <QList>
#include <QHash>
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
    void onClientDisconnected();

private:
    ClientHandler *findHandler(const QString &username) const;

    QTcpServer *m_server;
    QList<ClientHandler *> m_clients;
    QHash<QString, ClientHandler *> m_users;

    DatabaseWorker *m_dbWorker;
    QThread *m_dbThread;
};
