#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
class PileService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult listForStation(const QJsonObject &payload);
};
