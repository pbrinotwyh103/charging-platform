#include "services/reservationservice.h"
#include "services/servicehelpers.h"
#include "repositories/reservationrepository.h"
#include "repositories/orderrepository.h"
#include "repositories/pilerepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"

using namespace ServiceHelpers;
using Charging::ErrorCode;

namespace {
ServiceResult conflict(const QString &reason)
{
    return failure(ErrorCode::Conflict, QStringLiteral("当前状态不允许预约操作"), reason);
}

QJsonObject snapshot(const ReservationRecord &reservation, qint64 stationId)
{
    return {{"reservationId", double(reservation.id)}, {"userId", double(reservation.userId)},
        {"stationId", double(stationId)}, {"pileId", double(reservation.pileId)},
        {"status", reservation.status}, {"reservedAt", reservation.reservedAt},
        {"expiresAt", reservation.expiresAt}, {"usedAt", reservation.usedAt}};
}

ServiceResult userConflict(DatabaseManager *database, qint64 userId)
{
    QString error;
    UserRecord user;
    ReservationRecord reservation;
    OrderRecord order;
    if (!UserRepository(database).findById(userId, &user, &error)) return databaseError();
    if (!user.id) return failure(ErrorCode::NotFound, QStringLiteral("用户不存在"));
    if (user.status != "normal") return conflict("user_frozen");
    if (user.balanceCents <= 0) return conflict("insufficient_balance");
    if (!OrderRepository(database).findActiveByUser(userId, &order, &error)
        || !ReservationRepository(database).findActiveByUser(userId, &reservation, &error)) return databaseError();
    if (order.id) return conflict("order_conflict");
    if (reservation.id) return conflict("reservation_conflict");
    return {};
}
}

ServiceResult ReservationService::create(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    if (userId <= 0 || !identifier(payload.value("stationId"))
        || (payload.contains("pileId") && !identifier(payload.value("pileId")))
        || (payload.contains("durationMinutes") && !integer(payload.value("durationMinutes"), 5, 30)))
        return invalid();
    if (!ready(database())) return databaseError();
    QString error;
    ReservationRepository reservations(database());
    const auto now = QDateTime::currentDateTimeUtc();
    int expired = 0;
    if (!reservations.expireDue(now.toString(Qt::ISODate), &expired, &error)) return databaseError();
    const auto eligibility = userConflict(database(), userId);
    if (!eligibility.succeeded()) return eligibility;
    const auto stationId = qint64(payload.value("stationId").toDouble());
    StationRecord station;
    if (!StationRepository(database()).findById(stationId, &station, &error)) return databaseError();
    if (!station.id) return failure(ErrorCode::NotFound, QStringLiteral("充电站不存在"));
    if (station.status != "online") return conflict("pile_unavailable");
    PileRepository piles(database());
    QList<PileRecord> candidates;
    if (payload.contains("pileId")) {
        PileRecord pile;
        if (!piles.findById(qint64(payload.value("pileId").toDouble()), &pile, &error)) return databaseError();
        if (!pile.id) return failure(ErrorCode::NotFound, QStringLiteral("电桩不存在"));
        if (pile.stationId != stationId) return invalid();
        if (pile.status != "idle") return conflict("pile_unavailable");
        candidates.append(pile);
    } else if (!piles.listByStation(stationId, "idle", &candidates, &error)) {
        return databaseError();
    }
    for (const auto &pile : candidates) {
        qint64 reservationId = 0;
        const auto expiresAt = now.addSecs(payload.value("durationMinutes").toInt(15) * 60).toString(Qt::ISODate);
        if (reservations.create(userId, pile.id, expiresAt, &reservationId, &error)) {
            ReservationRecord reservation;
            if (!reservations.findById(reservationId, &reservation, &error)) return databaseError();
            ServiceResult result;
            result.payload = snapshot(reservation, stationId);
            return result;
        }
        // A competing transaction may have won after validation; report its business state.
        const auto raced = userConflict(database(), userId);
        if (!raced.succeeded()) return raced;
        PileRecord current;
        if (!piles.findById(pile.id, &current, &error)) return databaseError();
        if (current.id && current.status != "idle") continue;
        return databaseError();
    }
    return conflict("pile_unavailable");
}

ServiceResult ReservationService::cancel(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    if (userId <= 0 || !identifier(payload.value("reservationId"))) return invalid();
    if (!ready(database())) return databaseError();
    const auto id = qint64(payload.value("reservationId").toDouble());
    QString error;
    ReservationRepository reservations(database());
    ReservationRecord reservation;
    if (!reservations.findById(id, &reservation, &error)) return databaseError();
    if (!reservation.id) return failure(ErrorCode::NotFound, QStringLiteral("预约不存在"));
    if (reservation.userId != userId) return failure(ErrorCode::Forbidden, QStringLiteral("无权取消该预约"));
    const auto now = QDateTime::currentDateTimeUtc();
    if (reservation.status == "active"
        && QDateTime::fromString(reservation.expiresAt, Qt::ISODate) <= now) {
        int count = 0;
        if (!reservations.expireDue(now.toString(Qt::ISODate), &count, &error)) return databaseError();
        return conflict("reservation_expired");
    }
    if (reservation.status == "expired") return conflict("reservation_expired");
    if (reservation.status != "active" && reservation.status != "cancelled")
        return conflict("reservation_conflict");
    if (reservation.status == "active" && !reservations.cancel(id, userId, &error)) {
        if (!reservations.findById(id, &reservation, &error)) return databaseError();
        if (reservation.status == "expired") return conflict("reservation_expired");
        if (reservation.status == "used") return conflict("reservation_conflict");
        if (reservation.status != "cancelled") return databaseError();
    }
    if (!reservations.findById(id, &reservation, &error)) return databaseError();
    PileRecord pile;
    if (!PileRepository(database()).findById(reservation.pileId, &pile, &error)) return databaseError();
    ServiceResult result;
    result.payload = snapshot(reservation, pile.stationId);
    return result;
}

ServiceResult ReservationService::expireDue(const QDateTime &now)
{
    ConnectionCleanup cleanup(database());
    if (!now.isValid()) return invalid();
    if (!ready(database())) return databaseError();
    QString error;
    int count = 0;
    if (!ReservationRepository(database()).expireDue(now.toUTC().toString(Qt::ISODate), &count, &error))
        return databaseError();
    ServiceResult result;
    result.payload = {{"expiredCount", count}};
    return result;
}
