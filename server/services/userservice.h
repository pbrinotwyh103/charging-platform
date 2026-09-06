#pragma once
#include "services/servicebase.h"
#include "protocol/errorcodes.h"

#include <QJsonObject>
#include <QString>

struct UserServiceResult {
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    QJsonObject payload;

    bool succeeded() const { return error == Charging::ErrorCode::Success; }
};

class UserService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;

    UserServiceResult recharge(qint64 userId, const QJsonObject &payload);
    UserServiceResult walletLedger(qint64 userId, const QJsonObject &payload);
};
