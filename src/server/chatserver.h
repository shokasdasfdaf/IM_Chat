#pragma once

#include <QObject>
#include <QTcpServer>
#include <QList>
#include <QHash>

class ClientHandler;

class ChatServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    bool start(quint16 port = 8888);

private slots:
    void onNewConnection();
    void onLoginRequest(const QString &username);
    void onChatMessage(const QString &to, const QString &content);
    void onClientDisconnected();

private:
    void broadcastUserList();
    ClientHandler *findHandler(const QString &username) const;

    QTcpServer *m_server;
    QList<ClientHandler *> m_clients;
    QHash<QString, ClientHandler *> m_users; // username → handler
};
