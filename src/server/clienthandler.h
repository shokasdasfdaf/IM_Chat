#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientHandler(QTcpSocket *socket, QObject *parent = nullptr);
    QString username() const { return m_username; }
    void setUsername(const QString &name) { m_username = name; }

    void sendJson(const QJsonObject &json);

signals:
    void loginRequest(const QString &username);
    void chatMessage(const QString &to, const QString &content);
    void disconnected();

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket;
    QString m_username;
    QByteArray m_buffer;
};
