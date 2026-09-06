#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"

struct OrderRecord;
class OrderService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult active(qint64 userId);
    static QJsonObject snapshot(const OrderRecord &order);
};
