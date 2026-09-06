#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>
#include <functional>

struct AlarmRecord
{
    qint64 id = 0;
    qint64 pileId = 0;
    qint64 orderId = 0;
    QString alarmType;
    QString severity;
    QString message;
    QString status;
    QString occurredAt;
    QString recoveredAt;
    qint64 handledByAdminId = 0;
};

class AlarmRepository final : public RepositoryBase
{
  public:
    using RepositoryBase::RepositoryBase;

    bool count(const QString &status, int *total, QString *error) const;
    bool findById(qint64 alarmId, AlarmRecord *record, QString *error) const;
    // Status filtering and every visited value share one SQLite read snapshot.
    bool visitSnapshot(const QString &status, const std::function<bool(const AlarmRecord &)> &visitor,
                       QString *error) const;

    bool insert(const AlarmRecord &record, qint64 *alarmId, QString *error) const;
    bool list(const QString &status, int limit, int offset, QList<AlarmRecord> *records,
              QString *error) const;
    bool updateStatus(qint64 alarmId, const QString &status, qint64 adminId, QString *error) const;
};
