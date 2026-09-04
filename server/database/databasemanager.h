#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager final
{
public:
    DatabaseManager() = default;
    ~DatabaseManager();

    bool open(const QString &path, QString *error);
    QSqlDatabase database() const;

private:
    QString m_connectionName;
};
