#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>

class ChatClient : public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void login(const QString &username);
    void sendMessage(const QString &to, const QString &content);
    void requestHistory(const QString &keyword);
    bool isConnected() const;

signals:
    void loginSuccess(const QStringList &users);
    void loginFailed(const QString &reason);
    void userJoined(const QString &username);
    void userLeft(const QString &username);
    void messageReceived(const QString &from, const QString &content);
    void historyReceived(const QJsonArray &messages);
    void connectionError(const QString &error);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket;
};
