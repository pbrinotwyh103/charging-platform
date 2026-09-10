#include "services/orderservice.h"
#include "services/servicehelpers.h"
#include "repositories/orderrepository.h"
#include "repositories/userrepository.h"
#include <QJsonArray>

using namespace ServiceHelpers;

QJsonObject OrderService::snapshot(const OrderRecord &order)
{
    return {{"orderId", double(order.id)}, {"orderNo", order.orderNo}, {"seq", double(order.pushSequence)},
        {"userId", double(order.userId)}, {"stationId", double(order.stationId)},
        {"pileId", double(order.pileId)}, {"reservationId", double(order.reservationId)},
        {"status", order.status}, {"startedAt", order.startedAt}, {"stoppedAt", order.stoppedAt},
        {"durationSec", double(order.durationSeconds)}, {"durationSeconds", double(order.durationSeconds)},
        {"energyWh", double(order.energyWh)},
        {"energyKwh", double(order.energyWh) / 1000.0},
        {"stationName", order.stationName}, {"pileCode", order.pileCode},
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

ServiceResult OrderService::history(qint64 userId, const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    Pagination page;
    if (userId <= 0 || !pagination(payload, &page)) return invalid();
    if (!ready(database())) return databaseError();
    UserRecord user;
    QString error;
    if (!UserRepository(database()).findById(userId, &user, &error)) return databaseError();
    if (!user.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("用户不存在"));
    QList<OrderRecord> records;
    int total = 0;
    OrderRepository orders(database());
    if (!orders.countByUser(userId, &total, &error)
        || !orders.listByUser(userId, page.pageSize, page.offset, &records, &error))
        return databaseError();
    QJsonArray items;
    for (const auto &order : records) items.append(snapshot(order));
    ServiceResult result;
    result.payload = {{"items", items}, {"page", page.page},
                      {"pageSize", page.pageSize}, {"total", total}};
    return result;
}
