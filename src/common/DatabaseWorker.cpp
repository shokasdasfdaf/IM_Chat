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
        "  read INTEGER DEFAULT 0,"
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
        emit messageSaved(-1);
        return;
    }
    int msgId = query.lastInsertId().toInt();
    qDebug() << "Message saved:" << from << "->" << to << ":" << content << "id:" << msgId;
    emit messageSaved(msgId);
}

void DatabaseWorker::markRead(const QString &fromUser, const QString &toUser)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET read = 1 WHERE from_user = ? AND to_user = ? AND read = 0");
    query.addBindValue(fromUser);
    query.addBindValue(toUser);

    if (!query.exec()) {
        qWarning() << "Mark read failed:" << query.lastError().text();
    }
}

void DatabaseWorker::searchHistory(int requestId, const QString &keyword)
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, from_user, to_user, content, read, timestamp FROM messages "
        "WHERE content LIKE ? ESCAPE '\\' OR from_user LIKE ? ESCAPE '\\' "
        "ORDER BY timestamp DESC LIMIT 100"
    );
    // 转义 LIKE 的通配符 % _ \，避免用户输入 % 匹配全部
    QString escaped = keyword;
    escaped.replace('\\', "\\\\")
           .replace('%',  "\\%")
           .replace('_',  "\\_");
    QString pattern = "%" + escaped + "%";
    query.addBindValue(pattern);
    query.addBindValue(pattern);

    if (!query.exec()) {
        qWarning() << "Search history failed:" << query.lastError().text();
        emit historyResult(requestId, {});
        return;
    }

    qDebug() << "History search for:" << keyword << "requestId:" << requestId;
    QJsonArray results;
    while (query.next()) {
        QJsonObject msg;
        msg["id"] = query.value(0).toInt();
        msg["from"] = query.value(1).toString();
        msg["to"]   = query.value(2).toString();
        msg["content"] = query.value(3).toString();
        msg["read"] = query.value(4).toInt();
        msg["time"] = query.value(5).toString();
        results.append(msg);
    }
    emit historyResult(requestId, results);
}
