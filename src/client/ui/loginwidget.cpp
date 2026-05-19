#include "loginwidget.h"
#include "../chatclient.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QIntValidator>

LoginWidget::LoginWidget(ChatClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    auto *formLayout = new QFormLayout;

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("输入用户名");
    formLayout->addRow("用户名:", m_nameEdit);

    m_serverEdit = new QLineEdit("127.0.0.1");
    formLayout->addRow("服务器:", m_serverEdit);

    m_portEdit = new QLineEdit("8888");
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));
    formLayout->addRow("端口:", m_portEdit);

    m_loginBtn = new QPushButton("登录");
    m_statusLabel = new QLabel;

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_loginBtn);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch();

    connect(m_loginBtn, &QPushButton::clicked,
            this, &LoginWidget::onLoginClicked);

    connect(m_client, &ChatClient::loginSuccess, this, [this](const QStringList &users) {
        m_statusLabel->setText("登录成功");
        emit loginSucceeded(users);
    });
    connect(m_client, &ChatClient::loginFailed, this, [this](const QString &reason) {
        m_statusLabel->setText("登录失败: " + reason);
    });
    connect(m_client, &ChatClient::connectionError, this, [this](const QString &err) {
        m_statusLabel->setText("连接错误: " + err);
    });

    setWindowTitle("即时通讯 — 登录");
    setFixedSize(320, 220);
}

void LoginWidget::onLoginClicked()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_statusLabel->setText("请输入用户名");
        return;
    }

    m_statusLabel->setText("正在连接...");
    m_loginBtn->setEnabled(false);

    m_client->connectToServer(m_serverEdit->text().trimmed(),
                              m_portEdit->text().toUShort());
    m_client->login(name);
    m_loginBtn->setEnabled(true);
}
