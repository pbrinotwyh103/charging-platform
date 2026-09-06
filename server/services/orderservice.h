#pragma once
#include "protocol/errorcodes.h"
#include "services/servicebase.h"

#include <QJsonArray>
#include <QString>

struct OrderHistoryResult {
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    QJsonArray items;

    bool succeeded() const { return error == Charging::ErrorCode::Success; }
};

class OrderService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;

    OrderHistoryResult historyForUser(qint64 userId);
};
