#include "services/orderservice.h"
#include "services/servicehelpers.h"
#include "repositories/orderrepository.h"
#include "repositories/userrepository.h"

using namespace ServiceHelpers;

QJsonObject OrderService::snapshot(const OrderRecord &order)
{
    return {{"orderId", double(order.id)}, {"orderNo", order.orderNo}, {"seq", double(order.pushSequence)},
        {"userId", double(order.userId)}, {"stationId", double(order.stationId)},
        {"pileId", double(order.pileId)}, {"reservationId", double(order.reservationId)},
        {"status", order.status}, {"startedAt", order.startedAt}, {"stoppedAt", order.stoppedAt},
        {"durationSec", double(order.durationSeconds)}, {"energyWh", double(order.energyWh)},
        {"energyKwh", double(order.energyWh) / 1000.0},
        {"priceCentsPerKwh", double(order.unitPriceCents)},
        {"payableCents", double(order.feeCents)}, {"feeCents", double(order.feeCents)},
        {"stopReason", order.stopReason}, {"createdAt", order.createdAt}, {"updatedAt", order.updatedAt}};
}

ServiceResult OrderService::active(qint64 userId)
{
    ConnectionCleanup cleanup(database());
    if (userId <= 0) return invalid();
    if (!ready(database())) return databaseError();
    QString error;
    UserRecord user;
    if (!UserRepository(database()).findById(userId, &user, &error)) return databaseError();
    if (!user.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("用户不存在"));
    OrderRecord order;
    if (!OrderRepository(database()).findActiveByUser(userId, &order, &error)) return databaseError();
    ServiceResult result;
    if (order.id) result.payload = snapshot(order);
    result.payload.insert("active", order.id != 0);
    return result;
}
