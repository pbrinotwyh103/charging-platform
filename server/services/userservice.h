#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
class UserService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult updateProfile(qint64 userId, const QJsonObject &payload);
    ServiceResult recharge(qint64 userId, const QJsonObject &payload);
    ServiceResult walletLedger(qint64 userId, const QJsonObject &payload);
};
