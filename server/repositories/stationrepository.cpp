#include "repositories/stationrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {
void readStation(QSqlQuery &query, StationRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->name = query.value(1).toString();
    record->address = query.value(2).toString();
    record->longitude = query.value(3).toDouble();
    record->latitude = query.value(4).toDouble();
    record->priceCentsPerKwh = query.value(5).toLongLong();
    record->status = query.value(6).toString();
    record->totalPileCount = query.value(7).toInt();
    record->idlePileCount = query.value(8).toInt();
    record->onlinePileCount = query.value(9).toInt();
    record->createdAt = query.value(10).toString();
    record->updatedAt = query.value(11).toString();
}

QString stationSelect()
{
    return QStringLiteral(
        "SELECT s.id,s.name,s.address,s.longitude,s.latitude,s.price_cents_per_kwh,s.status,"
        "COUNT(p.id),COALESCE(SUM(CASE WHEN p.status='idle' THEN 1 ELSE 0 END),0),"
        "COALESCE(SUM(CASE WHEN p.status NOT IN ('offline','disabled') THEN 1 ELSE 0 END),0),"
        "s.created_at,s.updated_at FROM stations s "
        "LEFT JOIN charging_piles p ON p.station_id=s.id ");
}
}

bool StationRepository::findById(qint64 stationId, StationRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(stationSelect() + QStringLiteral("WHERE s.id=? GROUP BY s.id"));
    query.addBindValue(stationId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readStation(query, record);
    return true;
}

bool StationRepository::list(const QString &status, QList<StationRecord> *records,
                             QString *error) const
{
    records->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    QString sql = stationSelect();
    if (!status.isEmpty()) sql += QStringLiteral("WHERE s.status=? ");
    sql += QStringLiteral("GROUP BY s.id ORDER BY s.id");
    query.prepare(sql);
    if (!status.isEmpty()) query.addBindValue(status);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        StationRecord record;
        readStation(query, &record);
        records->append(record);
    }
    return true;
}

bool StationRepository::insert(const StationRecord &record, qint64 *stationId,
                               QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO stations(name,address,longitude,latitude,price_cents_per_kwh,status) "
        "VALUES(?,?,?,?,?,?)"));
    query.addBindValue(record.name.trimmed());
    query.addBindValue(record.address.trimmed());
    query.addBindValue(record.longitude);
    query.addBindValue(record.latitude);
    query.addBindValue(record.priceCentsPerKwh);
    query.addBindValue(record.status.isEmpty() ? QStringLiteral("online") : record.status);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (stationId) *stationId = query.lastInsertId().toLongLong();
    return true;
}

bool StationRepository::update(const StationRecord &record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE stations SET name=?,address=?,longitude=?,latitude=?,"
        "price_cents_per_kwh=?,status=?,updated_at=CURRENT_TIMESTAMP WHERE id=?"));
    query.addBindValue(record.name.trimmed());
    query.addBindValue(record.address.trimmed());
    query.addBindValue(record.longitude);
    query.addBindValue(record.latitude);
    query.addBindValue(record.priceCentsPerKwh);
    query.addBindValue(record.status);
    query.addBindValue(record.id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("充电站不存在");
        return false;
    }
    return true;
}
