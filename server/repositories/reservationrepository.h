#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct ReservationRecord {
    qint64 id = 0;
    qint64 userId = 0;
    qint64 pileId = 0;
    QString status;
    QString reservedAt;
    QString expiresAt;
    QString usedAt;
};

class ReservationRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool create(qint64 userId, qint64 pileId, const QString &expiresAt,
                qint64 *reservationId, QString *error) const;
    bool findActiveByUser(qint64 userId, ReservationRecord *record, QString *error) const;
    bool cancel(qint64 reservationId, qint64 userId, QString *error) const;
    bool markUsed(qint64 reservationId, QString *error) const;
    bool expireDue(const QString &now, int *expiredCount, QString *error) const;
};
