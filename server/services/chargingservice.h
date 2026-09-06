#pragma once
#include "services/servicebase.h"
#include "services/serviceresult.h"
#include "services/alarmservice.h"
#include "repositories/orderrepository.h"
#include "repositories/pilerepository.h"
#include "models/role.h"
#include "protocol/messagetypes.h"
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <functional>

struct ChargingEvent {
    Charging::MessageType type;
    qint64 userId;
    QJsonObject payload;
};

// Simulator/device adapter inputs. Rated current uses the simulator's 400 V supply.
struct ChargingSensorSample {
    bool connected = true;
    double temperatureC = 25;
    double currentA = 0;
    qint64 capacityWh = 60000;
};

class ChargingService final : public ServiceBase
{
public:
    using Clock = std::function<QDateTime()>;
    using SensorReader = std::function<ChargingSensorSample(const PileRecord &, const QDateTime &)>;
    explicit ChargingService(DatabaseManager *database = nullptr, Clock clock = {}, SensorReader sensors = {});
    ServiceResult start(qint64 userId, const QJsonObject &payload);
    ServiceResult stop(qint64 actorId, Charging::Role role, const QJsonObject &payload);
    // Drains queued stop/restore events as well as sampling active orders.
    QList<ChargingEvent> tick(const QDateTime &now);
    ServiceResult restore();

private:
    struct Session {
        OrderRecord order;
        qint64 seq = 0;
        qint64 baseDuration = 0;
        qint64 baseEnergy = 0;
        QDateTime updatedAt;
        bool stopping = false;
        QString finalStatus;
        QString reason;
        QString alarmType;
    };
    void remember(const OrderRecord &order);
    ServiceResult sample(Session &session, const QDateTime &now, const QString &requestedReason = {});
    ServiceResult settle(Session &session);
    QJsonObject snapshot(const Session &session) const;
    Clock m_clock;
    SensorReader m_sensors;
    QMutex m_mutex;
    QMap<qint64, Session> m_sessions;
    QList<ChargingEvent> m_pending;
};
