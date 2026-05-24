#include <QApplication>
#include "chatclient.h"
#include "ui/loginwidget.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ChatClient client;

    LoginWidget loginWidget(&client);
    MainWindow mainWindow(&client);

    QObject::connect(&loginWidget, &LoginWidget::loginSucceeded,
                     &mainWindow, [&](const QString &username, const QStringList &users) {
        mainWindow.setMyself(username);
        mainWindow.setUserList(users);
        mainWindow.show();
        loginWidget.hide();
    });

    QObject::connect(&client, &ChatClient::userJoined,
                     &mainWindow, &MainWindow::addUser);
    QObject::connect(&client, &ChatClient::userLeft,
                     &mainWindow, &MainWindow::removeUser);
    QObject::connect(&client, &ChatClient::connectionError,
                     &mainWindow, [&mainWindow](const QString &err) {
        mainWindow.setWindowTitle("即时通讯 — 连接已断开！");
    });
    QObject::connect(&client, &ChatClient::reconnecting,
                     &mainWindow, [&mainWindow] {
        mainWindow.setWindowTitle("即时通讯 — 重连中...");
    });
    QObject::connect(&client, &ChatClient::reconnected,
                     &mainWindow, [&mainWindow] {
        mainWindow.setWindowTitle("即时通讯");
    });

    loginWidget.show();
    return app.exec();
}
