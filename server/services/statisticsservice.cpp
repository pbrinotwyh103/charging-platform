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
bool addChecked(qint64 *sum, qint64 value)
{
    if (value < 0 || *sum > std::numeric_limits<qint64>::max() - value)
        return false;
    *sum += value;
    return true;
}
ServiceResult overflow()
{
    return failure(Charging::ErrorCode::DatabaseError, QStringLiteral("统计数值超出支持范围"),
                   "statistics_overflow");
}
QJsonObject pileCounts(const QList<PileRecord> &piles)
{
    QJsonObject result{{"available", 0}, {"reserved", 0}, {"charging", 0},
                       {"fault", 0},     {"offline", 0},  {"disabled", 0}};
    for (const auto &pile : piles)
    {
        const QString status = pile.status == "idle" ? QString("available") : pile.status;
        result.insert(status, result.value(status).toInteger() + 1);
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
    qint64 todayOrders = 0, monthOrders = 0, active = 0;
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
        if (!addChecked(&revenue, order.feeCents) || !addChecked(&energy, order.energyWh) ||
            (paid == today && !addChecked(&todayRevenue, order.feeCents)) ||
            (paid.year() == today.year() && paid.month() == today.month() &&
             !addChecked(&monthRevenue, order.feeCents)))
            return overflow();
    }
    const QJsonObject revenueMetrics{{"totalRevenueCents", revenue},
                                     {"todayRevenueCents", todayRevenue},
                                     {"monthRevenueCents", monthRevenue}};
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
    QMap<QDate, qint64> counts;
    for (const auto &order : orders)
    {
        ++counts[QDateTime::fromString(order.startedAt, Qt::ISODate).date()];
        if (settled(order) &&
            !addChecked(&revenue[QDateTime::fromString(order.stoppedAt, Qt::ISODate).date()], order.feeCents))
            return overflow();
    }
    QJsonArray points;
    const QDate today = QDateTime::currentDateTimeUtc().date();
    for (int i = days - 1; i >= 0; --i)
    {
        const auto date = today.addDays(-i);
        points.append(QJsonObject{{"date", date.toString(Qt::ISODate)},
                                  {"revenueCents", revenue.value(date)},
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
