#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"

class BillingService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    // Returns -1 for negative inputs or an amount outside qint64 range.
    static qint64 feeCents(qint64 energyWh, qint64 priceCentsPerKwh);
    // Internal settlement API; the calling charging service validates ownership.
    ServiceResult settle(qint64 orderId, qint64 durationSeconds, qint64 energyWh,
                         const QString &finalStatus, const QString &reason);
};
