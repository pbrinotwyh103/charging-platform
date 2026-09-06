#pragma once

#include "repositories/repositorybase.h"

#include <QString>
#include <QJsonObject>
#include <functional>

struct PileRecord;
struct ControlAttemptRecord
{
    qint64 id = 0;
    qint64 pileId = 0;
    qint64 orderId = 0;
    QString result;
    QJsonObject detail;
};

class ControlRecordRepository final : public RepositoryBase
{
  public:
    using RepositoryBase::RepositoryBase;

    bool finish(qint64 recordId, const QString &result, const QString &detail, QString *error) const;
    bool findAttempt(qint64 adminId, quint32 requestId, const QString &commandType, qint64 requestedPileId,
                     qint64 requestedOrderId, const QString &requestScope, ControlAttemptRecord *record,
                     QString *error) const;
    // Rechecks persisted activity while holding the SQLite writer lock.
    // Domain rejection: not_found or command_conflict; SQL failures use error.
    // persistResult writes the audit outcome on the same connection before commit.
    bool applyPileCommand(qint64 pileId, const QString &command,
                          const std::function<bool(const PileRecord &, QString *)> &persistResult,
                          QString *reason, QString *error) const;

    bool insert(qint64 adminId, qint64 pileId, qint64 orderId, const QString &commandType, quint32 requestId,
                const QString &result, const QString &detail, qint64 *recordId, QString *error) const;
};
