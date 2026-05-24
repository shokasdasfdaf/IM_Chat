#ifndef DATABASEWORKER_H
#define DATABASEWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QJsonArray>

class DatabaseWorker : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseWorker(QObject *parent = nullptr);
    ~DatabaseWorker();

public slots:
    void initialize();
    void saveMessage(const QString &from, const QString &to, const QString &content);
    void searchHistory(const QString &keyword);

signals:
    void initialized(bool ok);
    void messageSaved(bool ok);
    void historyResult(const QJsonArray &messages);

private:
    QSqlDatabase m_db;
};

#endif // DATABASEWORKER_H
