#include "services/billingservice.h"
#include "services/orderservice.h"
#include "services/servicehelpers.h"
#include "repositories/orderrepository.h"

using namespace ServiceHelpers;

qint64 BillingService::feeCents(qint64 energyWh, qint64 priceCentsPerKwh)
{
    if (energyWh < 0 || priceCentsPerKwh < 0) return -1;
    if (!energyWh || !priceCentsPerKwh) return 0;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    const qint64 wholeKwh = energyWh / 1000;
    if (wholeKwh > maximum / priceCentsPerKwh) return -1;
    const qint64 wholeFee = wholeKwh * priceCentsPerKwh;
    // Split both operands so every intermediate fits, even if Wh * price does not.
    const qint64 remainder = energyWh % 1000;
    const qint64 remainderFee = remainder * (priceCentsPerKwh / 1000)
        + remainder * (priceCentsPerKwh % 1000) / 1000;
    if (wholeFee > maximum - remainderFee) return -1;
    return wholeFee + remainderFee;
}

ServiceResult BillingService::settle(qint64 orderId, qint64 durationSeconds, qint64 energyWh,
                                    const QString &finalStatus, const QString &reason)
{
    ConnectionCleanup cleanup(database());
    if (orderId <= 0 || durationSeconds < 0 || energyWh < 0
        || (finalStatus != "completed" && finalStatus != "fault_stopped")
        || reason.trimmed().isEmpty()) return invalid();
    if (!ready(database())) return databaseError();
    QString error;
    OrderRepository orders(database());
    OrderRecord order;
    if (!orders.findById(orderId, &order, &error)) return databaseError();
    if (!order.id) return failure(Charging::ErrorCode::NotFound, QStringLiteral("订单不存在"));
    if (order.status != "charging" && order.status != "completed" && order.status != "fault_stopped")
        return failure(Charging::ErrorCode::Conflict, QStringLiteral("订单未在充电"), "order_not_active");
    const qint64 fee = feeCents(energyWh, order.unitPriceCents);
    if (fee < 0) return invalid();
    qint64 balance = 0;
    if (!orders.stopAndSettle(orderId, durationSeconds, energyWh, fee, finalStatus, reason, &balance, &error)) {
        if (error == QStringLiteral("钱包余额不足"))
            return failure(Charging::ErrorCode::Conflict, QStringLiteral("余额不足"), "insufficient_balance");
        return databaseError();
    }
    // Re-read the committed result: repeats must return the original final sample and fee.
    if (!orders.findById(orderId, &order, &error)) return databaseError();
    ServiceResult result;
    result.payload = OrderService::snapshot(order);
    result.payload.insert("balanceCents", double(balance));
    return result;
}
