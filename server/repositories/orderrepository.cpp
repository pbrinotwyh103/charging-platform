#include "repositories/orderrepository.h"

#include "database/databasemanager.h"

#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

bool OrderRepository::findHistoryByUser(qint64 userId, QJsonArray *items,
                                        QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT o.order_no, o.started_at, s.name, s.id, o.fee_cents, "
        "CASE "
        "  WHEN o.status = 'cancelled' THEN 'cancelled' "
        "  ELSE COALESCE(("
        "    SELECT wr.status FROM wallet_records wr "
        "    WHERE wr.order_id = o.id AND wr.record_type = 'charge_payment' "
        "    ORDER BY wr.created_at DESC, wr.id DESC LIMIT 1"
        "  ), 'unpaid') "
        "END AS payment_status, o.status "
        "FROM charging_orders o "
        "JOIN stations s ON s.id = o.station_id "
        "WHERE o.user_id = ? AND o.status <> 'charging' "
        "ORDER BY datetime(o.started_at) DESC, o.id DESC"));
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }

    QJsonArray result;
    while (query.next()) {
        result.append(QJsonObject{
            {QStringLiteral("orderNo"), query.value(0).toString()},
            {QStringLiteral("startedAt"), query.value(1).toString()},
            {QStringLiteral("stationName"), query.value(2).toString()},
            {QStringLiteral("stationId"), query.value(3).toLongLong()},
            {QStringLiteral("feeCents"), query.value(4).toLongLong()},
            {QStringLiteral("paymentStatus"), query.value(5).toString()},
            {QStringLiteral("orderStatus"), query.value(6).toString()}
        });
    }
    if (items) *items = result;
    return true;
}
