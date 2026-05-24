#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
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
    void reconnecting();
    void reconnected();

private slots:
    void onReadyRead();
    void onDisconnected();
    void onHeartbeat();
    void onReconnect();

private:
    void doLogin();

    QTcpSocket *m_socket = nullptr;
    QString m_pendingLogin;
    QString m_lastUsername;
    QString m_serverHost;
    quint16 m_serverPort = 0;

    QTimer *m_heartbeatTimer;
    int m_missedPongs = 0;

    QTimer *m_reconnectTimer;
    int m_reconnectCount = 0;
    bool m_reconnecting = false;
};
