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

    loginWidget.show();
    return app.exec();
}
