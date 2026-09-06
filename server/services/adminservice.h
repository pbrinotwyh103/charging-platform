#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
class ChargingService;
class AdminService final : public ServiceBase
{
  public:
    using ServiceBase::ServiceBase;
    // Dispatcher overwrites internal payload._requestScope with the connection
    // identity, because requestId values are only unique within a connection.
    ServiceResult execute(qint64 adminId, quint32 requestId, const QJsonObject &payload,
                          ChargingService *charging);

  private:
    using Handler = ServiceResult (AdminService::*)(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult statistics(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult stations(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult piles(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult users(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult orders(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult alarms(qint64, quint32, const QJsonObject &, ChargingService *);
    ServiceResult control(qint64, quint32, const QJsonObject &, ChargingService *);
};
