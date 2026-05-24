#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QHash>

class ChatClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(ChatClient *client, QWidget *parent = nullptr);

    void setMyself(const QString &username);
    void setUserList(const QStringList &users);
    void addUser(const QString &username);
    void removeUser(const QString &username);

private slots:
    void onSendMessage();
    void onUserSelected(QListWidgetItem *item);
    void onMessageReceived(const QString &from, const QString &content);

private:
    void switchToUser(const QString &username);

    ChatClient *m_client;
    QListWidget *m_userList;
    QStackedWidget *m_chatStack;
    QLineEdit *m_inputEdit;
    QPushButton *m_sendBtn;
    QHash<QString, QTextEdit *> m_chatPages;
    QString m_currentUser;
    QString m_myName;
};
