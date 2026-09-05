#include "repositories/orderrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString orderColumns()
{
    return QStringLiteral("id,order_no,user_id,station_id,pile_id,reservation_id,status,"
        "started_at,stopped_at,duration_seconds,energy_wh,unit_price_cents,fee_cents,"
        "stop_reason,created_at,updated_at");
}

void readOrder(QSqlQuery &query, OrderRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->orderNo = query.value(1).toString();
    record->userId = query.value(2).toLongLong();
    record->stationId = query.value(3).toLongLong();
    record->pileId = query.value(4).toLongLong();
    record->reservationId = query.value(5).toLongLong();
    record->status = query.value(6).toString();
    record->startedAt = query.value(7).toString();
    record->stoppedAt = query.value(8).toString();
    record->durationSeconds = query.value(9).toLongLong();
    record->energyWh = query.value(10).toLongLong();
    record->unitPriceCents = query.value(11).toLongLong();
    record->feeCents = query.value(12).toLongLong();
    record->stopReason = query.value(13).toString();
    record->createdAt = query.value(14).toString();
    record->updatedAt = query.value(15).toString();
}

bool rollback(QSqlDatabase &db, const QString &message, QString *error)
{
    db.rollback();
    if (error) *error = message;
    return false;
}
}

bool OrderRepository::createChargingOrder(const QString &orderNo, qint64 userId,
                                          qint64 pileId, qint64 reservationId,
                                          qint64 *orderId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery context(db);
    context.prepare(QStringLiteral(
        "SELECT p.station_id,p.status,s.price_cents_per_kwh,u.status "
        "FROM charging_piles p JOIN stations s ON s.id=p.station_id "
        "JOIN users u ON u.id=? WHERE p.id=?"));
    context.addBindValue(userId);
    context.addBindValue(pileId);
    if (!context.exec() || !context.next()) {
        return rollback(db, context.lastError().isValid() ? context.lastError().text()
                                                         : QStringLiteral("用户或电桩不存在"), error);
    }
    if (context.value(3).toString() != QStringLiteral("normal")) {
        return rollback(db, QStringLiteral("冻结用户不能开始充电"), error);
    }
    const QString expectedPileStatus = reservationId > 0
        ? QStringLiteral("reserved") : QStringLiteral("idle");
    if (context.value(1).toString() != expectedPileStatus) {
        return rollback(db, QStringLiteral("电桩当前不可开始充电"), error);
    }
    if (reservationId > 0) {
        QSqlQuery reservation(db);
        reservation.prepare(QStringLiteral(
            "UPDATE reservations SET status='used',used_at=CURRENT_TIMESTAMP "
            "WHERE id=? AND user_id=? AND pile_id=? AND status='active' "
            "AND expires_at>CURRENT_TIMESTAMP"));
        reservation.addBindValue(reservationId);
        reservation.addBindValue(userId);
        reservation.addBindValue(pileId);
        if (!reservation.exec() || reservation.numRowsAffected() != 1) {
            return rollback(db, reservation.lastError().isValid()
                ? reservation.lastError().text() : QStringLiteral("预约无效或已过期"), error);
        }
    }
    QSqlQuery occupy(db);
    occupy.prepare(QStringLiteral(
        "UPDATE charging_piles SET status='charging',updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status=?"));
    occupy.addBindValue(pileId);
    occupy.addBindValue(expectedPileStatus);
    if (!occupy.exec() || occupy.numRowsAffected() != 1) {
        return rollback(db, occupy.lastError().isValid() ? occupy.lastError().text()
                                                         : QStringLiteral("电桩状态已变化"), error);
    }
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO charging_orders(order_no,user_id,station_id,pile_id,reservation_id,"
        "status,started_at,unit_price_cents) VALUES(?,?,?,?,?,'charging',CURRENT_TIMESTAMP,?)"));
    insert.addBindValue(orderNo);
    insert.addBindValue(userId);
    insert.addBindValue(context.value(0));
    insert.addBindValue(pileId);
    if (reservationId > 0) insert.addBindValue(reservationId);
    else insert.addBindValue(QVariant());
    insert.addBindValue(context.value(2));
    if (!insert.exec()) return rollback(db, insert.lastError().text(), error);
    const qint64 id = insert.lastInsertId().toLongLong();
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (orderId) *orderId = id;
    return true;
}

bool OrderRepository::findById(qint64 orderId, OrderRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1 FROM charging_orders WHERE id=?").arg(orderColumns()));
    query.addBindValue(orderId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readOrder(query, record);
    return true;
}

bool OrderRepository::findActiveByUser(qint64 userId, OrderRecord *record,
                                       QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT %1 FROM charging_orders WHERE user_id=? AND status='charging' LIMIT 1")
        .arg(orderColumns()));
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readOrder(query, record);
    return true;
}

bool OrderRepository::updateProgress(qint64 orderId, qint64 durationSeconds,
                                     qint64 energyWh, qint64 feeCents,
                                     QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE charging_orders SET duration_seconds=?,energy_wh=?,fee_cents=?,"
        "updated_at=CURRENT_TIMESTAMP WHERE id=? AND status='charging'"));
    query.addBindValue(qMax<qint64>(0, durationSeconds));
    query.addBindValue(qMax<qint64>(0, energyWh));
    query.addBindValue(qMax<qint64>(0, feeCents));
    query.addBindValue(orderId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("进行中的订单不存在");
        return false;
    }
    return true;
}

bool OrderRepository::listByUser(qint64 userId, int limit, int offset,
                                 QList<OrderRecord> *records, QString *error) const
{
    records->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT %1 FROM charging_orders WHERE user_id=? ORDER BY created_at DESC LIMIT ? OFFSET ?")
        .arg(orderColumns()));
    query.addBindValue(userId);
    query.addBindValue(qBound(1, limit, 200));
    query.addBindValue(qMax(0, offset));
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        OrderRecord record;
        readOrder(query, &record);
        records->append(record);
    }
    return true;
}
