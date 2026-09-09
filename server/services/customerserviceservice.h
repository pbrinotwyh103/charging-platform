#pragma once

#include "services/servicebase.h"
#include "services/serviceresult.h"

class CustomerServiceService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;
    ServiceResult ask(qint64 userId, const QJsonObject &payload);

private:
    static QString offlineAnswer(const QString &question);
};
