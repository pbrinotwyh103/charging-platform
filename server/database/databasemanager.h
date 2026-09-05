#pragma once

#include <QSqlDatabase>
#include <QMutex>
#include <QSet>
#include <QString>

class DatabaseManager final
{
public:
    DatabaseManager() = default;
    ~DatabaseManager();

    bool open(const QString &path, QString *error);
    bool initializeSchema(QString *error);
    QSqlDatabase database(QString *error = nullptr) const;
    void releaseCurrentThreadConnection();
    bool checkIntegrity(QString *error = nullptr) const;
    bool backupTo(const QString &backupPath, QString *error = nullptr) const;
    bool restoreFrom(const QString &backupPath, QString *error = nullptr);
    int schemaVersion(QString *error = nullptr) const;
    QString databasePath() const { return m_databasePath; }

private:
    bool configureConnection(QSqlDatabase &database, QString *error) const;
    bool executeScript(QSqlDatabase &database, const QString &resourcePath, QString *error) const;
    bool applyMigrations(QSqlDatabase &database, QString *error) const;
    bool checkIntegrity(QSqlDatabase &database, QString *error) const;
    void closeAllConnections();
    QString currentConnectionName() const;

    QString m_databasePath;
    QString m_mainConnectionName;
    quintptr m_mainThreadId = 0;
    mutable QMutex m_connectionMutex;
    mutable QSet<QString> m_workerConnections;
};
