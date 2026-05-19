#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class ChatClient;

class LoginWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWidget(ChatClient *client, QWidget *parent = nullptr);

signals:
    void loginSucceeded(const QStringList &users);

private slots:
    void onLoginClicked();

private:
    ChatClient *m_client;
    QLineEdit *m_nameEdit;
    QLineEdit *m_serverEdit;
    QLineEdit *m_portEdit;
    QPushButton *m_loginBtn;
    QLabel *m_statusLabel;
};
