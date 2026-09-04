#include "database/databasemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

DatabaseManager::~DatabaseManager()
{
    if (m_mainConnectionName.isEmpty()) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(m_mainConnectionName, false);
        if (db.isValid()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_mainConnectionName);
}

bool DatabaseManager::open(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("无法创建数据库目录：%1").arg(info.absolutePath());
        return false;
    }

    m_databasePath = info.absoluteFilePath();
    m_mainThreadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    m_mainConnectionName = QStringLiteral("charging_server_main_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_mainConnectionName);
    db.setDatabaseName(m_databasePath);
    if (!configureConnection(db, error)) {
        return false;
    }
    return initializeSchema(error);
}

bool DatabaseManager::initializeSchema(QString *error)
{
    QSqlDatabase db = database(error);
    if (!db.isValid() || !db.isOpen()) {
        return false;
    }
    if (!executeScript(db, QStringLiteral(":/database/schema.sql"), error)
        || !executeScript(db, QStringLiteral(":/database/seed.sql"), error)) {
        return false;
    }

    // 兼容框架阶段已经生成的旧数据库。
    bool hasPermissions = false;
    QSqlQuery columns(db);
    if (!columns.exec(QStringLiteral("PRAGMA table_info(admins)"))) {
        if (error) *error = columns.lastError().text();
        return false;
    }
    while (columns.next()) {
        if (columns.value(1).toString() == QStringLiteral("permissions")) {
            hasPermissions = true;
            break;
        }
    }
    if (!hasPermissions) {
        QSqlQuery migration(db);
        if (!migration.exec(QStringLiteral(
                "ALTER TABLE admins ADD COLUMN permissions TEXT NOT NULL DEFAULT 'all'"))) {
            if (error) *error = migration.lastError().text();
            return false;
        }
    }
    return true;
}

QSqlDatabase DatabaseManager::database(QString *error) const
{
    const QString name = currentConnectionName();
    if (QSqlDatabase::contains(name)) {
        return QSqlDatabase::database(name);
    }

    QMutexLocker locker(&m_connectionMutex);
    if (QSqlDatabase::contains(name)) {
        return QSqlDatabase::database(name);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(m_databasePath);
    if (!configureConnection(db, error)) {
        return {};
    }
    m_workerConnections.insert(name);
    return db;
}

void DatabaseManager::releaseCurrentThreadConnection()
{
    const QString name = currentConnectionName();
    if (name == m_mainConnectionName) {
        return;
    }
    {
        QMutexLocker locker(&m_connectionMutex);
        m_workerConnections.remove(name);
    }
    if (QSqlDatabase::contains(name)) {
        {
            QSqlDatabase db = QSqlDatabase::database(name, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
}

bool DatabaseManager::configureConnection(QSqlDatabase &db, QString *error) const
{
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

bool DatabaseManager::executeScript(QSqlDatabase &db,
                                    const QString &resourcePath,
                                    QString *error) const
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("无法读取数据库脚本：%1").arg(resourcePath);
        return false;
    }

    QString cleaned;
    const QString script = QString::fromUtf8(file.readAll());
    const QStringList lines = script.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int comment = line.indexOf(QStringLiteral("--"));
        cleaned += (comment >= 0 ? line.left(comment) : line);
        cleaned += QLatin1Char('\n');
    }

    const QStringList statements = cleaned.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    for (const QString &statement : statements) {
        const QString sql = statement.trimmed();
        if (sql.isEmpty()) {
            continue;
        }
        QSqlQuery query(db);
        if (!query.exec(sql)) {
            db.rollback();
            if (error) *error = QStringLiteral("%1：%2").arg(query.lastError().text(), sql.left(80));
            return false;
        }
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

QString DatabaseManager::currentConnectionName() const
{
    const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    if (threadId == m_mainThreadId) {
        return m_mainConnectionName;
    }
    return QStringLiteral("charging_server_worker_%1_%2")
        .arg(m_mainConnectionName)
        .arg(threadId, 0, 16);
}
