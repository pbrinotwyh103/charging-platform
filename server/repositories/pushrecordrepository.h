#pragma once

#include "repositories/repositorybase.h"

#include <QString>

class PushRecordRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool insert(const QString &targetRole, qint64 targetId, int messageType,
                quint32 requestId, const QString &result,
                qint64 *recordId, QString *error) const;
};
