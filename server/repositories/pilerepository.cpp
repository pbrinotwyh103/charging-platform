#include "repositories/pilerepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {
void readPile(QSqlQuery &query, PileRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->stationId = query.value(1).toLongLong();
    record->pileCode = query.value(2).toString();
    record->chargeType = query.value(3).toString();
    record->powerKw = query.value(4).toDouble();
    record->status = query.value(5).toString();
    record->totalChargeCount = query.value(6).toInt();
    record->totalChargeSeconds = query.value(7).toLongLong();
    record->lastHeartbeatAt = query.value(8).toString();
    record->updatedAt = query.value(9).toString();
}

QString pileColumns()
{
    return QStringLiteral("id,station_id,pile_code,charge_type,power_kw,status,"
                          "total_charge_count,total_charge_seconds,last_heartbeat_at,updated_at");
}
}

bool PileRepository::findById(qint64 pileId, PileRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1 FROM charging_piles WHERE id=?").arg(pileColumns()));
    query.addBindValue(pileId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readPile(query, record);
    return true;
}

bool PileRepository::listByStation(qint64 stationId, const QString &status,
                                   QList<PileRecord> *records, QString *error) const
{
    records->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    QString sql = QStringLiteral("SELECT %1 FROM charging_piles WHERE station_id=?").arg(pileColumns());
    if (!status.isEmpty()) sql += QStringLiteral(" AND status=?");
    sql += QStringLiteral(" ORDER BY pile_code");
    query.prepare(sql);
    query.addBindValue(stationId);
    if (!status.isEmpty()) query.addBindValue(status);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        PileRecord record;
        readPile(query, &record);
        records->append(record);
    }
    return true;
}

bool PileRepository::insert(const PileRecord &record, qint64 *pileId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_piles(station_id,pile_code,charge_type,power_kw,status) "
        "VALUES(?,?,?,?,?)"));
    query.addBindValue(record.stationId);
    query.addBindValue(record.pileCode.trimmed());
    query.addBindValue(record.chargeType);
    query.addBindValue(record.powerKw);
    query.addBindValue(record.status.isEmpty() ? QStringLiteral("idle") : record.status);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (pileId) *pileId = query.lastInsertId().toLongLong();
    return true;
}

bool PileRepository::updateStatus(qint64 pileId, const QString &expectedStatus,
                                  const QString &newStatus, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    QString sql = QStringLiteral(
        "UPDATE charging_piles SET status=?,updated_at=CURRENT_TIMESTAMP WHERE id=?");
    if (!expectedStatus.isEmpty()) sql += QStringLiteral(" AND status=?");
    query.prepare(sql);
    query.addBindValue(newStatus);
    query.addBindValue(pileId);
    if (!expectedStatus.isEmpty()) query.addBindValue(expectedStatus);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("电桩状态已变化或电桩不存在");
        return false;
    }
    return true;
}

bool PileRepository::updateHeartbeat(qint64 pileId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE charging_piles SET last_heartbeat_at=CURRENT_TIMESTAMP,"
        "updated_at=CURRENT_TIMESTAMP WHERE id=?"));
    query.addBindValue(pileId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("电桩不存在");
        return false;
    }
    return true;
}
