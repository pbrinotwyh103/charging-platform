#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
class AlarmService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult raiseOnce(qint64 pileId, qint64 orderId, const QString &type,
                            const QString &severity, const QString &message);
};
