#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
class StationService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult nearby(qint64 userId, const QJsonObject &payload);
    ServiceResult toggleFavorite(qint64 userId, const QJsonObject &payload);
};
