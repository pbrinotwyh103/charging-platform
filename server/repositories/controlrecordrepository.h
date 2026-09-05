#pragma once

#include "repositories/repositorybase.h"

#include <QString>

class ControlRecordRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool insert(qint64 adminId, qint64 pileId, qint64 orderId,
                const QString &commandType, quint32 requestId,
                const QString &result, const QString &detail,
                qint64 *recordId, QString *error) const;
};
