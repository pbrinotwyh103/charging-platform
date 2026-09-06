#include "services/chargingservice.h"
#include "services/billingservice.h"
#include "services/orderservice.h"
#include "services/servicehelpers.h"
#include "repositories/reservationrepository.h"
#include "repositories/userrepository.h"
#include "repositories/stationrepository.h"
#include <QMutexLocker>
#include <QUuid>

using namespace ServiceHelpers;
using Charging::ErrorCode;

namespace {
ServiceResult conflict(const QString &reason)
{
    return failure(ErrorCode::Conflict, QStringLiteral("当前状态不允许充电操作"), reason);
}
qint64 nominalEnergy(double powerKw, qint64 duration)
{
    const long double energy = std::floor(static_cast<long double>(powerKw) * 1000 * duration / 3600);
    return energy < 0 || energy >= std::ldexp(1.0L, 63) ? -1 : qint64(energy);
}
}

ChargingService::ChargingService(DatabaseManager *database, Clock clock, SensorReader sensors)
    : ServiceBase(database), m_clock(clock ? std::move(clock) : Clock(QDateTime::currentDateTimeUtc)),
      m_sensors(sensors ? std::move(sensors) : SensorReader([](const PileRecord &pile, const QDateTime &) {
          ChargingSensorSample sample;
          sample.currentA = pile.powerKw * 1000 / 400;
          return sample;
      }))
{
}

void ChargingService::remember(const OrderRecord &order)
{
    if (m_sessions.contains(order.id)) return;
    Session session;
    session.order = order;
    session.baseDuration = order.durationSeconds;
    session.baseEnergy = order.energyWh;
    session.updatedAt = QDateTime::fromString(order.startedAt, Qt::ISODate).addSecs(order.durationSeconds);
    m_sessions.insert(order.id, session);
}

ServiceResult ChargingService::start(qint64 userId, const QJsonObject &payload)
{
    QMutexLocker lock(&m_mutex);
    ConnectionCleanup cleanup(database());
    if (userId <= 0 || !identifier(payload.value("reservationId"))) return invalid();
    if (!ready(database())) return databaseError();
    QString error;
    const qint64 reservationId = qint64(payload.value("reservationId").toDouble());
    ReservationRecord reservation;
    if (!ReservationRepository(database()).findById(reservationId, &reservation, &error)) return databaseError();
    if (!reservation.id) return failure(ErrorCode::NotFound, QStringLiteral("预约不存在"));
    if (reservation.userId != userId) return failure(ErrorCode::Forbidden, QStringLiteral("无权使用该预约"));
    OrderRepository orders(database());
    OrderRecord order;
    if (!orders.findActiveByUser(userId, &order, &error)) return databaseError();
    if (order.id) {
        remember(order);
        ServiceResult result;
        result.payload = snapshot(m_sessions[order.id]);
        return result;
    }
    if (reservation.status == "expired" || (reservation.status == "active"
        && QDateTime::fromString(reservation.expiresAt, Qt::ISODate) <= QDateTime::currentDateTimeUtc()))
        return conflict("reservation_expired");
    if (reservation.status != "active") return conflict("reservation_conflict");
    UserRecord user;
    PileRecord pile;
    StationRecord station;
    if (!UserRepository(database()).findById(userId, &user, &error)
        || !PileRepository(database()).findById(reservation.pileId, &pile, &error)
        || !StationRepository(database()).findById(pile.stationId, &station, &error)) return databaseError();
    if (user.status != "normal") return conflict("user_frozen");
    if (user.balanceCents <= 0) return conflict("insufficient_balance");
    if (pile.status != "reserved" || station.status != "online") return conflict("pile_unavailable");
    qint64 orderId = 0;
    if (!orders.createChargingOrder(QUuid::createUuid().toString(QUuid::WithoutBraces), userId,
                                    pile.id, reservationId, &orderId, &error)) {
        if (error == "冻结用户不能开始充电") return conflict("user_frozen");
        if (error == "insufficient_balance" || error == "user_frozen") return conflict(error);
        if (error == "station_unavailable") return conflict("pile_unavailable");
        if (error == "预约无效或已过期") return conflict("reservation_expired");
        if (error == "电桩当前不可开始充电" || error == "电桩状态已变化") return conflict("pile_unavailable");
        return databaseError();
    }
    if (!orders.findById(orderId, &order, &error)) return databaseError();
    remember(order);
    ServiceResult result;
    result.payload = snapshot(m_sessions[order.id]);
    return result;
}

QJsonObject ChargingService::snapshot(const Session &session) const
{
    auto payload = OrderService::snapshot(session.order);
    payload.insert("status", session.stopping ? "stopping" : session.order.status);
    payload.insert("seq", double(session.seq));
    payload.insert("updatedAt", session.updatedAt.toUTC().toString(Qt::ISODate));
    return payload;
}

ServiceResult ChargingService::sample(Session &session, const QDateTime &now, const QString &requestedReason)
{
    if (session.stopping) return {};
    if (!now.isValid()) return invalid();
    QString error;
    PileRecord pile;
    UserRecord user;
    if (!PileRepository(database()).findById(session.order.pileId, &pile, &error)
        || !UserRepository(database()).findById(session.order.userId, &user, &error)) return databaseError();
    if (!pile.id || !user.id) return databaseError();
    const auto started = QDateTime::fromString(session.order.startedAt, Qt::ISODate);
    if (!started.isValid() || !std::isfinite(pile.powerKw) || pile.powerKw <= 0) return invalid();
    const auto sensor = m_sensors(pile, now);
    OrderRecord candidate = session.order;
    candidate.durationSeconds = qMax(session.order.durationSeconds, qMax<qint64>(0, started.secsTo(now)));
    const qint64 total = nominalEnergy(pile.powerKw, candidate.durationSeconds);
    const qint64 base = nominalEnergy(pile.powerKw, session.baseDuration);
    if (total < 0 || base < 0 || total - base > std::numeric_limits<qint64>::max() - session.baseEnergy) return invalid();
    candidate.energyWh = qMax(session.order.energyWh, session.baseEnergy + total - base);
    QString reason = requestedReason;
    QString alarm;
    if (requestedReason == "connection_lost" || !sensor.connected) alarm = "connection_lost";
    else if (pile.status == "fault") alarm = "device_fault";
    else if (pile.status == "offline") alarm = "device_offline";
    else if (pile.status != "charging") alarm = "connection_lost";
    else if (!std::isfinite(sensor.temperatureC) || sensor.temperatureC > 60) alarm = "over_temperature";
    else if (!std::isfinite(sensor.currentA) || sensor.currentA < 0 || sensor.currentA > pile.powerKw * 1000 / 400) alarm = "over_current";
    if (sensor.capacityWh > 0 && candidate.energyWh >= sensor.capacityWh) {
        candidate.energyWh = qMax(session.order.energyWh, sensor.capacityWh);
        reason = "fully_charged";
    }
    candidate.feeCents = BillingService::feeCents(candidate.energyWh, candidate.unitPriceCents);
    if (candidate.feeCents < 0) return invalid();
    if (candidate.feeCents >= user.balanceCents) {
        // Largest whole Wh affordable at this tariff; never overshoot the wallet.
        qint64 low = 0, high = candidate.energyWh;
        while (low < high) {
            const auto middle = low + (high - low) / 2 + 1;
            if (BillingService::feeCents(middle, candidate.unitPriceCents) <= user.balanceCents) low = middle;
            else high = middle - 1;
        }
        candidate.energyWh = low;
        candidate.feeCents = BillingService::feeCents(low, candidate.unitPriceCents);
        reason = "insufficient_balance";
    }
    if (!alarm.isEmpty()) reason = alarm;
    if (reason.isEmpty()) {
        if (!OrderRepository(database()).updateProgress(candidate.id, candidate.durationSeconds,
            candidate.energyWh, candidate.feeCents, &error)) return databaseError();
    }
    session.order = candidate;
    session.updatedAt = qMax(session.updatedAt, now.toUTC());
    if (!reason.isEmpty()) {
        session.stopping = true;
        session.reason = reason;
        session.alarmType = alarm;
        session.finalStatus = alarm.isEmpty() ? "completed" : "fault_stopped";
    }
    ServiceResult result;
    result.payload = snapshot(session);
    result.payload.insert("powerKw", pile.powerKw);
    return result;
}

ServiceResult ChargingService::settle(Session &session)
{
    if (!session.alarmType.isEmpty()) {
        const auto alarm = AlarmService(database()).raiseOnce(session.order.pileId, session.order.id,
            session.alarmType, "critical", QStringLiteral("充电异常：%1").arg(session.alarmType));
        if (!alarm.succeeded()) return alarm;
        if (alarm.payload.value("created").toBool())
            m_pending.append({Charging::MessageType::AlarmPush, session.order.userId, alarm.payload});
        session.alarmType.clear();
    }
    auto result = BillingService(database()).settle(session.order.id, session.order.durationSeconds,
        session.order.energyWh, session.finalStatus, session.reason);
    if (result.succeeded()) {
        result.payload.insert("seq", double(++session.seq));
        result.payload.insert("updatedAt", session.updatedAt.toUTC().toString(Qt::ISODate));
        m_pending.append({Charging::MessageType::ChargingStoppedPush, session.order.userId, result.payload});
    }
    return result;
}

ServiceResult ChargingService::stop(qint64 actorId, Charging::Role role, const QJsonObject &payload)
{
    QMutexLocker lock(&m_mutex);
    ConnectionCleanup cleanup(database());
    if (role != Charging::Role::User && role != Charging::Role::Administrator)
        return failure(ErrorCode::Forbidden, QStringLiteral("无权停止充电"));
    if (actorId <= 0 || !identifier(payload.value("orderId"))) return invalid();
    if (!ready(database())) return databaseError();
    OrderRecord order;
    QString error;
    if (!OrderRepository(database()).findById(qint64(payload.value("orderId").toDouble()), &order, &error)) return databaseError();
    if (!order.id) return failure(ErrorCode::NotFound, QStringLiteral("订单不存在"));
    if (role == Charging::Role::User && order.userId != actorId)
        return failure(ErrorCode::Forbidden, QStringLiteral("无权停止该订单"));
    if (order.status != "charging")
        return BillingService(database()).settle(order.id, order.durationSeconds, order.energyWh, "completed", "user_stop");
    remember(order);
    auto &session = m_sessions[order.id];
    auto result = sample(session, m_clock(), role == Charging::Role::Administrator ? "admin_stop" : "user_stop");
    if (!result.succeeded()) return result;
    result = settle(session);
    if (result.succeeded()) m_sessions.remove(order.id);
    return result;
}

QList<ChargingEvent> ChargingService::tick(const QDateTime &now)
{
    QMutexLocker lock(&m_mutex);
    ConnectionCleanup cleanup(database());
    if (ready(database()) && now.isValid()) {
        for (auto it = m_sessions.begin(); it != m_sessions.end();) {
            auto result = sample(it.value(), now);
            if (!result.succeeded()) { ++it; continue; }
            if (it->stopping) {
                result = settle(it.value());
                if (result.succeeded()) { it = m_sessions.erase(it); continue; }
            } else {
                result.payload.insert("seq", double(++it->seq));
                m_pending.append({Charging::MessageType::ChargingProgressPush, it->order.userId, result.payload});
            }
            ++it;
        }
    }
    QList<ChargingEvent> events;
    events.swap(m_pending);
    return events;
}

ServiceResult ChargingService::restore()
{
    QMutexLocker lock(&m_mutex);
    ConnectionCleanup cleanup(database());
    if (!ready(database())) return databaseError();
    QString error;
    QList<OrderRecord> orders;
    if (!OrderRepository(database()).listActive(&orders, &error)) return databaseError();
    for (const auto &order : orders) remember(order);
    for (const auto &order : orders) {
        PileRecord pile;
        if (!PileRepository(database()).findById(order.pileId, &pile, &error)) return databaseError();
        if (!pile.id || pile.status != "charging") {
            auto result = sample(m_sessions[order.id], m_clock(), "connection_lost");
            if (!result.succeeded()) return result;
            result = settle(m_sessions[order.id]);
            if (!result.succeeded()) return result;
            m_sessions.remove(order.id);
        }
    }
    return {};
}
