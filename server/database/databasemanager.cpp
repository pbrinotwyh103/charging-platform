#include "database/databasemanager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

DatabaseManager::~DatabaseManager()
{
    if (m_connectionName.isEmpty()) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::open(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("无法创建数据库目录：%1").arg(info.absolutePath());
        return false;
    }

    m_connectionName = QStringLiteral("server_main");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(info.absoluteFilePath());
    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        if (error) *error = query.lastError().text();
        return false;
    }
    query.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    query.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    return true;
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName);
}
