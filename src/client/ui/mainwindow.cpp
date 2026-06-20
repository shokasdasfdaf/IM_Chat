#include "mainwindow.h"
#include "../chatclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QMenu>
#include <QWidgetAction>
#include <QJsonArray>
#include <QJsonObject>

MainWindow::MainWindow(ChatClient *client, QWidget *parent)
    : QMainWindow(parent), m_client(client)
{
    setWindowTitle("即时通讯");
    resize(800, 500);

    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *splitter = new QSplitter(Qt::Horizontal);

    m_userList = new QListWidget;
    splitter->addWidget(m_userList);

    m_chatStack = new QStackedWidget;
    splitter->addWidget(m_chatStack);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter);

    auto *searchLayout = new QHBoxLayout;
    auto *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("搜索聊天记录...");
    auto *searchBtn = new QPushButton("搜索");
    searchLayout->addWidget(searchEdit);
    searchLayout->addWidget(searchBtn);
    mainLayout->addLayout(searchLayout);

    auto *inputLayout = new QHBoxLayout;
    m_emojiBtn = new QPushButton("😀");
    m_emojiBtn->setFixedWidth(36);
    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText("输入消息，Enter 发送");
    m_sendBtn = new QPushButton("发送");
    inputLayout->addWidget(m_emojiBtn);
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(m_sendBtn);
    mainLayout->addLayout(inputLayout);

    connect(m_emojiBtn, &QPushButton::clicked,
            this, &MainWindow::onEmojiClicked);
    connect(m_userList, &QListWidget::itemClicked,
            this, &MainWindow::onUserSelected);
    connect(m_sendBtn, &QPushButton::clicked,
            this, &MainWindow::onSendMessage);
    connect(m_inputEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onSendMessage);
    connect(m_client, &ChatClient::messageReceived,
            this, &MainWindow::onMessageReceived);
    connect(m_client, &ChatClient::historyReceived,
            this, [this](const QJsonArray &results) {
        QString text = "=== 搜索结果 ===<br>";
        for (const QJsonValue &v : results) {
            QJsonObject msg = v.toObject();
            text += "[" + msg["time"].toString() + "] "
                 + msg["from"].toString() + " -> " + msg["to"].toString()
                 + ": " + msg["content"].toString() + "<br>";
        }
        if (results.isEmpty())
            text += "无结果";
        switchToUser("搜索结果");
        m_chatPages["搜索结果"]->setHtml(text);
    });
    connect(searchBtn, &QPushButton::clicked, this, [this, searchEdit]() {
        QString kw = searchEdit->text().trimmed();
        if (!kw.isEmpty())
            m_client->requestHistory(kw);
    });
    connect(searchEdit, &QLineEdit::returnPressed, this, [searchBtn]() {
        searchBtn->click();
    });
}

void MainWindow::setMyself(const QString &username)
{
    m_myName = username;
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
    for (int i = 0; i < m_userList->count(); ++i) {
        if (m_userList->item(i)->text().remove(" (未读)") == username)
            return;
    }
    m_userList->addItem(username);
}

void MainWindow::removeUser(const QString &username)
{
    for (int i = 0; i < m_userList->count(); ++i) {
        if (m_userList->item(i)->text().remove(" (未读)") == username) {
            delete m_userList->takeItem(i);
            break;
        }
    }
}

void MainWindow::onUserSelected(QListWidgetItem *item)
{
    QString username = item->text().remove(" (未读)");
    switchToUser(username);
}

void MainWindow::switchToUser(const QString &username)
{
    m_currentUser = username;

    if (!m_chatPages.contains(username)) {
        auto *chat = new QTextEdit;
        chat->setReadOnly(true);
        m_chatPages.insert(username, chat);
        m_chatStack->addWidget(chat);
    }

    m_chatStack->setCurrentWidget(m_chatPages[username]);
    setWindowTitle("即时通讯 — 与 " + username + " 聊天中");

    // 清除未读标记并发送 ACK
    if (m_unreadUsers.contains(username)) {
        m_unreadUsers.remove(username);
        updateUserListItem(username);
        m_client->sendAck(username);
    }
}

void MainWindow::onSendMessage()
{
    QString content = m_inputEdit->text().trimmed();
    if (content.isEmpty() || m_currentUser.isEmpty())
        return;

    m_client->sendMessage(m_currentUser, content);

    auto *chat = m_chatPages.value(m_currentUser);
    if (chat)
        chat->append("我: " + content);

    m_inputEdit->clear();
}

void MainWindow::onMessageReceived(const QString &from, const QString &content, int msgId)
{
    Q_UNUSED(msgId);

    // 确保聊天页存在
    if (!m_chatPages.contains(from)) {
        auto *chat = new QTextEdit;
        chat->setReadOnly(true);
        m_chatPages.insert(from, chat);
        m_chatStack->addWidget(chat);
    }

    auto *chat = m_chatPages.value(from);
    if (chat)
        chat->append(from + ": " + content);

    // 当前正在和这个人聊天，自动标记已读
    if (m_currentUser == from) {
        m_client->sendAck(from);
    } else {
        m_unreadUsers.insert(from);
        updateUserListItem(from);
    }
}

void MainWindow::updateUserListItem(const QString &username)
{
    for (int i = 0; i < m_userList->count(); ++i) {
        QListWidgetItem *item = m_userList->item(i);
        QString baseName = item->text().remove(" (未读)");
        if (baseName == username) {
            if (m_unreadUsers.contains(username))
                item->setText(username + " (未读)");
            else
                item->setText(username);
            return;
        }
    }
}

void MainWindow::onEmojiClicked()
{
    const QStringList emojis = {
        "😀", "😃", "😄", "😁", "😅", "😂", "🤣", "😊", "😇", "🙂",
        "😉", "😍", "😘", "😗", "😋", "😛", "😜", "🤔", "🤗", "😎",
        "😕", "😟", "😔", "😢", "😭", "😤", "😡", "😈", "💀", "👍",
        "👎", "👌", "✌", "🤞", "👏", "👋", "🙌", "🙏", "💪", "🎉",
        "🎊", "🎂", "💕", "💔", "💖", "💗", "❤", "💙", "💚", "💛"
    };

    auto *menu = new QMenu(this);
    auto *grid = new QWidget;
    auto *gridLayout = new QGridLayout(grid);
    gridLayout->setSpacing(2);
    gridLayout->setContentsMargins(4, 4, 4, 4);

    for (int i = 0; i < emojis.size(); ++i) {
        auto *btn = new QPushButton(emojis[i]);
        btn->setFixedSize(32, 28);
        btn->setStyleSheet("QPushButton { border: none; font-size: 16px; }"
                           "QPushButton:hover { background: #d0d0d0; border-radius: 4px; }");
        connect(btn, &QPushButton::clicked, this, [this, emoji = emojis[i]]() {
            m_inputEdit->insert(emoji);
        });
        gridLayout->addWidget(btn, i / 10, i % 10);
    }

    auto *action = new QWidgetAction(menu);
    action->setDefaultWidget(grid);
    menu->addAction(action);
    menu->exec(m_emojiBtn->mapToGlobal(QPoint(0, m_emojiBtn->height())));
    menu->deleteLater();
}
