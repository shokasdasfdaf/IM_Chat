#include <QCoreApplication>
#include <QDebug>
#include "chatserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ChatServer server;
    if (!server.start(8888)) {
        qCritical() << "Failed to start server";
        return 1;
    }

    qDebug() << "Chat server running on port 8888...";
    return app.exec();
}
