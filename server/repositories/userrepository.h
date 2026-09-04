#pragma once

#include "repositories/repositorybase.h"

#include <QString>

struct UserRecord {
    qint64 id = 0;
    QString phone;
    QString nickname;
    QString avatarPath;
    qint64 balanceCents = 0;
    QString status;
};

class UserRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool findByPhone(const QString &phone, UserRecord *record, QString *error) const;
    bool findById(qint64 id, UserRecord *record, QString *error) const;
    bool findOrCreate(const QString &phone, UserRecord *record, bool *created,
                      QString *error) const;
};
