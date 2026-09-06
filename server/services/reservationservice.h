#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
#include <QDateTime>

class ReservationService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult create(qint64 userId, const QJsonObject &payload);
    ServiceResult cancel(qint64 userId, const QJsonObject &payload);
    ServiceResult expireDue(const QDateTime &now);
};
