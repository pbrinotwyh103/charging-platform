#pragma once

#include "repositories/repositorybase.h"

#include <QString>

struct AdminRecord {
    qint64 id = 0;
    QString username;
    QString passwordHash;
    QString passwordSalt;
    QString permissions;
    QString status;
};

class AdminRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool findByUsername(const QString &username, AdminRecord *record, QString *error) const;
    bool insert(const QString &username, const QString &passwordHash,
                const QString &passwordSalt, const QString &permissions,
                QString *error) const;
    bool updateLastLogin(qint64 adminId, QString *error) const;
};
