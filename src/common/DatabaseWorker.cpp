#include "DatabaseWorker.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>

DatabaseWorker::DatabaseWorker(QObject *parent)
    : QObject(parent)
{
}

DatabaseWorker::~DatabaseWorker()
{
    if (m_db.isOpen())
        m_db.close();
}

void DatabaseWorker::initialize()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "chat_db");
    m_db.setDatabaseName("chat_history.db");

    if (!m_db.open()) {
        qWarning() << "Database open failed:" << m_db.lastError().text();
        emit initialized(false);
        return;
    }

    QSqlQuery query(m_db);
    query.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT UNIQUE NOT NULL,"
        "  created_at TEXT DEFAULT (datetime('now','localtime'))"
        ")"
    );

    query.exec(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  from_user TEXT NOT NULL,"
        "  to_user TEXT NOT NULL,"
        "  content TEXT NOT NULL,"
        "  timestamp TEXT DEFAULT (datetime('now','localtime'))"
        ")"
    );

    qDebug() << "Database initialized";
    emit initialized(true);
}

void DatabaseWorker::saveMessage(const QString &from, const QString &to, const QString &content)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO messages (from_user, to_user, content) VALUES (?, ?, ?)");
    query.addBindValue(from);
    query.addBindValue(to);
    query.addBindValue(content);

    if (!query.exec()) {
        qWarning() << "Save message failed:" << query.lastError().text();
        emit messageSaved(false);
        return;
    }
    qDebug() << "Message saved:" << from << "->" << to << ":" << content;
    emit messageSaved(true);
}

void DatabaseWorker::searchHistory(const QString &keyword)
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT from_user, to_user, content, timestamp FROM messages "
        "WHERE content LIKE ? OR from_user LIKE ? "
        "ORDER BY timestamp DESC LIMIT 100"
    );
    QString pattern = "%" + keyword + "%";
    query.addBindValue(pattern);
    query.addBindValue(pattern);

    if (!query.exec()) {
        qWarning() << "Search history failed:" << query.lastError().text();
        emit historyResult({});
        return;
    }

    qDebug() << "History search for:" << keyword;
    QJsonArray results;
    while (query.next()) {
        QJsonObject msg;
        msg["from"] = query.value(0).toString();
        msg["to"]   = query.value(1).toString();
        msg["content"] = query.value(2).toString();
        msg["time"] = query.value(3).toString();
        results.append(msg);
    }
    emit historyResult(results);
}
