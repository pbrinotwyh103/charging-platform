#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct OrderRecord {
    qint64 id = 0;
    QString orderNo;
    qint64 userId = 0;
    qint64 stationId = 0;
    qint64 pileId = 0;
    qint64 reservationId = 0;
    QString status;
    QString startedAt;
    QString stoppedAt;
    qint64 durationSeconds = 0;
    qint64 energyWh = 0;
    qint64 unitPriceCents = 0;
    qint64 feeCents = 0;
    QString stopReason;
    QString createdAt;
    QString updatedAt;
};

class OrderRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool createChargingOrder(const QString &orderNo, qint64 userId, qint64 pileId,
                             qint64 reservationId, qint64 *orderId, QString *error) const;
    bool findById(qint64 orderId, OrderRecord *record, QString *error) const;
    bool findActiveByUser(qint64 userId, OrderRecord *record, QString *error) const;
    bool updateProgress(qint64 orderId, qint64 durationSeconds, qint64 energyWh,
                        qint64 feeCents, QString *error) const;
    bool listByUser(qint64 userId, int limit, int offset,
                    QList<OrderRecord> *records, QString *error) const;
};
