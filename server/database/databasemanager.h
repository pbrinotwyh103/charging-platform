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

private:
    bool configureConnection(QSqlDatabase &database, QString *error) const;
    bool executeScript(QSqlDatabase &database, const QString &resourcePath, QString *error) const;
    QString currentConnectionName() const;

    QString m_databasePath;
    QString m_mainConnectionName;
    quintptr m_mainThreadId = 0;
    mutable QMutex m_connectionMutex;
    mutable QSet<QString> m_workerConnections;
};
