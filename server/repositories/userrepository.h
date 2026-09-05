#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct UserRecord {
    qint64 id = 0;
    QString phone;
    QString nickname;
    QString avatarPath;
    qint64 balanceCents = 0;
    QString status;
    QString createdAt;
    QString updatedAt;
};

class UserRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool findByPhone(const QString &phone, UserRecord *record, QString *error) const;
    bool findById(qint64 id, UserRecord *record, QString *error) const;
    bool findOrCreate(const QString &phone, UserRecord *record, bool *created,
                      QString *error) const;
    bool updateProfile(qint64 userId, const QString &nickname,
                       const QString &avatarPath, QString *error) const;
    bool setStatus(qint64 userId, const QString &status, QString *error) const;
    bool search(const QString &phoneKeyword, int limit, int offset,
                QList<UserRecord> *records, QString *error) const;
};
