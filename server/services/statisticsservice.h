#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
class StatisticsService final : public ServiceBase
{
  public:
    using ServiceBase::ServiceBase;
    ServiceResult summary(const QJsonObject &payload);
    ServiceResult revenueTrend(const QJsonObject &payload);
    ServiceResult pileStatus(const QJsonObject &payload);
};
