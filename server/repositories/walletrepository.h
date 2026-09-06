#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct WalletRecord {
    qint64 id = 0;
    QString recordNo;
    qint64 userId = 0;
    qint64 orderId = 0;
    QString recordType;
    qint64 amountCents = 0;
    qint64 balanceAfterCents = 0;
    QString status;
    QString createdAt;
};

class WalletRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool recharge(const QString &recordNo, qint64 userId, qint64 amountCents,
                  qint64 *balanceAfterCents, QString *error) const;
    bool settleOrder(const QString &recordNo, qint64 orderId, qint64 durationSeconds,
                     qint64 energyWh, qint64 feeCents, const QString &finalStatus,
                     const QString &stopReason, qint64 *balanceAfterCents,
                     QString *error) const;
    bool listByUser(qint64 userId, int limit, int offset,
                    QList<WalletRecord> *records, QString *error) const;
};
