#include "database/databasemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

DatabaseManager::~DatabaseManager()
{
    closeAllConnections();
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
        db = {};
        QSqlDatabase::removeDatabase(m_mainConnectionName);
        m_mainConnectionName.clear();
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
        || !applyMigrations(db, error)
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
    return checkIntegrity(db, error);
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

bool DatabaseManager::checkIntegrity(QString *error) const
{
    QSqlDatabase db = database(error);
    return db.isValid() && db.isOpen() && checkIntegrity(db, error);
}

bool DatabaseManager::backupTo(const QString &backupPath, QString *error) const
{
    const QFileInfo destination(backupPath);
    if (destination.absoluteFilePath() == QFileInfo(m_databasePath).absoluteFilePath()) {
        if (error) *error = QStringLiteral("备份路径不能与当前数据库相同");
        return false;
    }
    if (!QDir().mkpath(destination.absolutePath())) {
        if (error) *error = QStringLiteral("无法创建备份目录：%1").arg(destination.absolutePath());
        return false;
    }

    QSqlDatabase db = database(error);
    if (!db.isValid() || !db.isOpen() || !checkIntegrity(db, error)) {
        return false;
    }
    QSqlQuery checkpoint(db);
    if (!checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"))) {
        if (error) *error = checkpoint.lastError().text();
        return false;
    }
    while (checkpoint.next()) {}
    checkpoint.finish();

    if (QFile::exists(destination.absoluteFilePath())
        && !QFile::remove(destination.absoluteFilePath())) {
        if (error) *error = QStringLiteral("无法覆盖已有备份：%1").arg(destination.absoluteFilePath());
        return false;
    }
    QString escaped = destination.absoluteFilePath();
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    QSqlQuery backup(db);
    if (!backup.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped))) {
        if (error) *error = backup.lastError().text();
        return false;
    }
    return QFileInfo(destination.absoluteFilePath()).size() > 0;
}

bool DatabaseManager::restoreFrom(const QString &backupPath, QString *error)
{
    const QFileInfo source(backupPath);
    if (!source.isFile()) {
        if (error) *error = QStringLiteral("备份文件不存在：%1").arg(backupPath);
        return false;
    }

    const QString verificationName = QStringLiteral("charging_restore_verify_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool validBackup = false;
    {
        QSqlDatabase verification = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               verificationName);
        verification.setDatabaseName(source.absoluteFilePath());
        if (verification.open()) {
            validBackup = checkIntegrity(verification, error);
            verification.close();
        } else if (error) {
            *error = verification.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(verificationName);
    if (!validBackup) {
        return false;
    }

    QFile input(source.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = input.errorString();
        return false;
    }
    const QByteArray bytes = input.readAll();
    input.close();
    if (bytes.isEmpty()) {
        if (error) *error = QStringLiteral("备份文件为空");
        return false;
    }

    closeAllConnections();
    QSaveFile output(m_databasePath);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size()
        || !output.commit()) {
        if (error) *error = output.errorString();
        return false;
    }
    QFile::remove(m_databasePath + QStringLiteral("-wal"));
    QFile::remove(m_databasePath + QStringLiteral("-shm"));

    m_mainThreadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    m_mainConnectionName = QStringLiteral("charging_server_main_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_mainConnectionName);
    db.setDatabaseName(m_databasePath);
    return configureConnection(db, error) && initializeSchema(error);
}

int DatabaseManager::schemaVersion(QString *error) const
{
    QSqlDatabase db = database(error);
    if (!db.isValid() || !db.isOpen()) return -1;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_version"))
        || !query.next()) {
        if (error) *error = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
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

bool DatabaseManager::applyMigrations(QSqlDatabase &db, QString *error) const
{
    struct Migration { int version; const char *resource; };
    const Migration migrations[] = {
        {3, ":/database/migrations/003_repository_indexes.sql"}
    };

    QSqlQuery versionQuery(db);
    if (!versionQuery.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_version"))
        || !versionQuery.next()) {
        if (error) *error = versionQuery.lastError().text();
        return false;
    }
    int currentVersion = versionQuery.value(0).toInt();
    for (const Migration &migration : migrations) {
        if (migration.version <= currentVersion) continue;
        if (!executeScript(db, QString::fromLatin1(migration.resource), error)) {
            return false;
        }
        QSqlQuery record(db);
        record.prepare(QStringLiteral("INSERT INTO schema_version(version) VALUES(?)"));
        record.addBindValue(migration.version);
        if (!record.exec()) {
            if (error) *error = record.lastError().text();
            return false;
        }
        currentVersion = migration.version;
    }
    return true;
}

bool DatabaseManager::checkIntegrity(QSqlDatabase &db, QString *error) const
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA quick_check"))) {
        if (error) *error = query.lastError().text();
        return false;
    }
    QStringList failures;
    while (query.next()) {
        const QString result = query.value(0).toString();
        if (result.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
            failures.append(result);
        }
    }
    if (!failures.isEmpty()) {
        if (error) *error = QStringLiteral("数据库完整性检查失败：%1").arg(failures.join(QStringLiteral("；")));
        return false;
    }
    return true;
}

void DatabaseManager::closeAllConnections()
{
    QStringList names;
    {
        QMutexLocker locker(&m_connectionMutex);
        names = m_workerConnections.values();
        m_workerConnections.clear();
    }
    if (!m_mainConnectionName.isEmpty()) names.append(m_mainConnectionName);
    for (const QString &name : names) {
        if (!QSqlDatabase::contains(name)) continue;
        {
            QSqlDatabase db = QSqlDatabase::database(name, false);
            if (db.isValid()) db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
    m_mainConnectionName.clear();
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
