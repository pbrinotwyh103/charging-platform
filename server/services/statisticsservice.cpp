#include "services/statisticsservice.h"
#include "services/adminservicehelpers.h"
#include "repositories/userrepository.h"
#include "repositories/alarmrepository.h"
#include <QDateTime>
#include <QMap>

using namespace ServiceHelpers;
using namespace AdminServiceHelpers;

namespace
{
QJsonObject pileCounts(const QList<PileRecord> &piles)
{
    QJsonObject result{{"available", 0}, {"reserved", 0}, {"charging", 0},
                       {"fault", 0},     {"offline", 0},  {"disabled", 0}};
    for (const auto &pile : piles)
    {
        const QString status = pile.status == "idle" ? QString("available") : pile.status;
        result.insert(status, result.value(status).toInt() + 1);
    }
    result.insert("idle", result.value("available")); // Existing chart uses the database name.
    result.insert("total", piles.size());
    return result;
}
bool settled(const OrderRecord &order)
{
    return order.status == "completed" || order.status == "fault_stopped";
}
} // namespace

ServiceResult StatisticsService::summary(const QJsonObject &)
{
    ConnectionCleanup cleanup(database());
    if (!ready(database()))
        return databaseError();
    QList<OrderRecord> orders;
    QList<PileRecord> piles;
    QString error;
    int users = 0, stations = 0, openAlarms = 0;
    if (!allOrders(database(), &orders, &error) || !allPiles(database(), &piles, &error) ||
        !UserRepository(database()).count({}, &users, &error) ||
        !StationRepository(database()).count({}, &stations, &error) ||
        !AlarmRepository(database()).count("open", &openAlarms, &error))
        return databaseError();
    const QDate today = QDateTime::currentDateTimeUtc().date();
    qint64 revenue = 0, todayRevenue = 0, monthRevenue = 0, energy = 0;
    int todayOrders = 0, monthOrders = 0, active = 0;
    for (const auto &order : orders)
    {
        const QDate date = QDateTime::fromString(order.startedAt, Qt::ISODate).date();
        if (date == today)
            ++todayOrders;
        if (date.year() == today.year() && date.month() == today.month())
            ++monthOrders;
        if (order.status == "charging")
            ++active;
        if (!settled(order))
            continue;
        const QDate paid = QDateTime::fromString(order.stoppedAt, Qt::ISODate).date();
        revenue += order.feeCents;
        energy += order.energyWh;
        if (paid == today)
            todayRevenue += order.feeCents;
        if (paid.year() == today.year() && paid.month() == today.month())
            monthRevenue += order.feeCents;
    }
    const QJsonObject revenueMetrics{{"totalRevenueCents", double(revenue)},
                                     {"todayRevenueCents", double(todayRevenue)},
                                     {"monthRevenueCents", double(monthRevenue)}};
    const QJsonObject orderMetrics{{"totalOrderCount", orders.size()},
                                   {"todayOrderCount", todayOrders},
                                   {"monthOrderCount", monthOrders},
                                   {"activeOrderCount", active}};
    ServiceResult result;
    result.payload = revenueMetrics;
    for (auto it = orderMetrics.begin(); it != orderMetrics.end(); ++it)
        result.payload.insert(it.key(), it.value());
    result.payload.insert("revenueMetrics", revenueMetrics);
    result.payload.insert("orderMetrics", orderMetrics);
    result.payload.insert("pileStatus", pileCounts(piles));
    result.payload.insert("userCount", users);
    result.payload.insert("stationCount", stations);
    result.payload.insert("openAlarmCount", openAlarms);
    result.payload.insert("energyKwh", double(energy) / 1000);
    result.payload.insert("updatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return result;
}

ServiceResult StatisticsService::revenueTrend(const QJsonObject &payload)
{
    ConnectionCleanup cleanup(database());
    if (payload.contains("days") && !integer(payload.value("days"), 1, 366))
        return invalid();
    if (!ready(database()))
        return databaseError();
    const int days = payload.value("days").toInt(7);
    QList<OrderRecord> orders;
    QString error;
    if (!allOrders(database(), &orders, &error))
        return databaseError();
    QMap<QDate, qint64> revenue;
    QMap<QDate, int> counts;
    for (const auto &order : orders)
    {
        ++counts[QDateTime::fromString(order.startedAt, Qt::ISODate).date()];
        if (settled(order))
            revenue[QDateTime::fromString(order.stoppedAt, Qt::ISODate).date()] += order.feeCents;
    }
    QJsonArray points;
    const QDate today = QDateTime::currentDateTimeUtc().date();
    for (int i = days - 1; i >= 0; --i)
    {
        const auto date = today.addDays(-i);
        points.append(QJsonObject{{"date", date.toString(Qt::ISODate)},
                                  {"revenueCents", double(revenue.value(date))},
                                  {"orderCount", counts.value(date)}});
    }
    ServiceResult result;
    result.payload = {{"days", days}, {"points", points}, {"items", points}};
    return result;
}

ServiceResult StatisticsService::pileStatus(const QJsonObject &)
{
    ConnectionCleanup cleanup(database());
    if (!ready(database()))
        return databaseError();
    QList<PileRecord> piles;
    QString error;
    if (!allPiles(database(), &piles, &error))
        return databaseError();
    ServiceResult result;
    result.payload = pileCounts(piles);
    return result;
}
