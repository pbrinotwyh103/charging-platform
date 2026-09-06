#pragma once

#include "repositories/repositorybase.h"

#include <QString>

class ControlRecordRepository final : public RepositoryBase
{
  public:
    using RepositoryBase::RepositoryBase;

    bool finish(qint64 recordId, const QString &result, const QString &detail, QString *error) const;
    bool findSuccess(qint64 adminId, quint32 requestId, const QString &commandType, qint64 pileId,
                     qint64 orderId, const QString &requestScope, QString *detail, QString *error) const;
    // Rechecks persisted activity while holding the SQLite writer lock.
    // Domain rejection: not_found or command_conflict; SQL failures use error.
    bool applyPileCommand(qint64 pileId, const QString &command, QString *reason, QString *error) const;

    bool insert(qint64 adminId, qint64 pileId, qint64 orderId, const QString &commandType, quint32 requestId,
                const QString &result, const QString &detail, qint64 *recordId, QString *error) const;
};
