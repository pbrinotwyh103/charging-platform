#include "repositories/alarmrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
void readAlarm(QSqlQuery &query, AlarmRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->pileId = query.value(1).toLongLong();
    record->orderId = query.value(2).toLongLong();
    record->alarmType = query.value(3).toString();
    record->severity = query.value(4).toString();
    record->message = query.value(5).toString();
    record->status = query.value(6).toString();
    record->occurredAt = query.value(7).toString();
    record->recoveredAt = query.value(8).toString();
    record->handledByAdminId = query.value(9).toLongLong();
}
}

bool AlarmRepository::insert(const AlarmRecord &record, qint64 *alarmId,
                             QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO alarms(pile_id,order_id,alarm_type,severity,message,status) "
        "VALUES(?,?,?,?,?,?)"));
    query.addBindValue(record.pileId > 0 ? QVariant(record.pileId) : QVariant());
    query.addBindValue(record.orderId > 0 ? QVariant(record.orderId) : QVariant());
    query.addBindValue(record.alarmType);
    query.addBindValue(record.severity);
    query.addBindValue(record.message);
    query.addBindValue(record.status.isEmpty() ? QStringLiteral("open") : record.status);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (alarmId) *alarmId = query.lastInsertId().toLongLong();
    return true;
}

bool AlarmRepository::list(const QString &status, int limit, int offset,
                           QList<AlarmRecord> *records, QString *error) const
{
    records->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    QString sql = QStringLiteral(
        "SELECT id,pile_id,order_id,alarm_type,severity,message,status,occurred_at,"
        "recovered_at,handled_by_admin_id FROM alarms");
    if (!status.isEmpty()) sql += QStringLiteral(" WHERE status=?");
    sql += QStringLiteral(" ORDER BY occurred_at DESC,id DESC LIMIT ? OFFSET ?");
    query.prepare(sql);
    if (!status.isEmpty()) query.addBindValue(status);
    query.addBindValue(qBound(1, limit, 200));
    query.addBindValue(qMax(0, offset));
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        AlarmRecord record;
        readAlarm(query, &record);
        records->append(record);
    }
    return true;
}

bool AlarmRepository::updateStatus(qint64 alarmId, const QString &status,
                                   qint64 adminId, QString *error) const
{
    if (status != QStringLiteral("acknowledged") && status != QStringLiteral("resolved")) {
        if (error) *error = QStringLiteral("无效的告警状态");
        return false;
    }
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE alarms SET status=?,handled_by_admin_id=?,"
        "recovered_at=CASE WHEN ?='resolved' THEN CURRENT_TIMESTAMP ELSE recovered_at END "
        "WHERE id=? AND status!='resolved'"));
    query.addBindValue(status);
    query.addBindValue(adminId > 0 ? QVariant(adminId) : QVariant());
    query.addBindValue(status);
    query.addBindValue(alarmId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("告警不存在或已经关闭");
        return false;
    }
    return true;
}
