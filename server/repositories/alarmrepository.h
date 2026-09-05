#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct AlarmRecord {
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

    bool insert(const AlarmRecord &record, qint64 *alarmId, QString *error) const;
    bool list(const QString &status, int limit, int offset,
              QList<AlarmRecord> *records, QString *error) const;
    bool updateStatus(qint64 alarmId, const QString &status, qint64 adminId,
                      QString *error) const;
};
