#include "mainwindow.h"
#include "../chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>

MainWindow::MainWindow(ChatClient *client, QWidget *parent)
    : QMainWindow(parent), m_client(client)
{
    setWindowTitle("即时通讯");
    resize(800, 500);

    // 中央部件
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // 上下分屏：用户列表+聊天区域 / 输入区
    auto *splitter = new QSplitter(Qt::Horizontal);

    m_userList = new QListWidget;
    splitter->addWidget(m_userList);

    m_chatStack = new QStackedWidget;
    splitter->addWidget(m_chatStack);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter);

    // 输入区
    auto *inputLayout = new QHBoxLayout;
    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText("输入消息，Enter 发送");
    m_sendBtn = new QPushButton("发送");
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(m_sendBtn);
    mainLayout->addLayout(inputLayout);

    connect(m_userList, &QListWidget::itemClicked,
            this, &MainWindow::onUserSelected);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &MainWindow::onSendMessage);
    connect(m_inputEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onSendMessage);
    connect(m_client, &ChatClient::messageReceived,
            this, &MainWindow::onMessageReceived);
}

void MainWindow::setUserList(const QStringList &users)
{
    m_userList->clear();
    for (const QString &user : users)
        addUser(user);
}

void MainWindow::addUser(const QString &username)
{
    if (username == m_myName)
        return;
    // 检查是否已存在
    for (int i = 0; i < m_userList->count(); ++i) {
        if (m_userList->item(i)->text() == username)
            return;
    }
    m_userList->addItem(username);
}

void MainWindow::removeUser(const QString &username)
{
    for (int i = 0; i < m_userList->count(); ++i) {
        if (m_userList->item(i)->text() == username) {
            delete m_userList->takeItem(i);
            break;
        }
    }
}

void MainWindow::onUserSelected(QListWidgetItem *item)
{
    switchToUser(item->text());
}

void MainWindow::switchToUser(const QString &username)
{
    m_currentUser = username;

    // 没有该用户的聊天页则创建
    if (!m_chatPages.contains(username)) {
        auto *chat = new QTextEdit;
        chat->setReadOnly(true);
        m_chatPages.insert(username, chat);
        m_chatStack->addWidget(chat);
    }

    m_chatStack->setCurrentWidget(m_chatPages[username]);
    setWindowTitle("即时通讯 — 与 " + username + " 聊天中");
}

void MainWindow::onSendMessage()
{
    QString content = m_inputEdit->text().trimmed();
    if (content.isEmpty() || m_currentUser.isEmpty())
        return;

    m_client->sendMessage(m_currentUser, content);

    // 显示自己发的消息
    auto *chat = m_chatPages.value(m_currentUser);
    if (chat)
        chat->append("我: " + content);

    m_inputEdit->clear();
}

void MainWindow::onMessageReceived(const QString &from, const QString &content)
{
    switchToUser(from);
    auto *chat = m_chatPages.value(from);
    if (chat)
        chat->append(from + ": " + content);
}
