#include "repositories/controlrecordrepository.h"

#include "database/databasemanager.h"
#include "repositories/pilerepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>

bool ControlRecordRepository::insert(qint64 adminId, qint64 pileId, qint64 orderId,
                                     const QString &commandType, quint32 requestId, const QString &result,
                                     const QString &detail, qint64 *recordId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO device_control_records(admin_id,pile_id,order_id,command_type,"
        "request_id,result,detail) VALUES((SELECT id FROM admins WHERE id=?),"
        "(SELECT id FROM charging_piles WHERE id=?),(SELECT id FROM charging_orders WHERE id=?),?,?,?,?)"));
    query.addBindValue(adminId > 0 ? QVariant(adminId) : QVariant());
    query.addBindValue(pileId > 0 ? QVariant(pileId) : QVariant());
    query.addBindValue(orderId > 0 ? QVariant(orderId) : QVariant());
    query.addBindValue(commandType);
    query.addBindValue(requestId);
    query.addBindValue(result);
    query.addBindValue(detail);
    if (!query.exec())
    {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    if (recordId)
        *recordId = query.lastInsertId().toLongLong();
    return true;
}

bool ControlRecordRepository::finish(qint64 recordId, const QString &result, const QString &detail,
                                     QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE device_control_records SET result=?,detail=? WHERE id=?"));
    query.addBindValue(result);
    query.addBindValue(detail);
    query.addBindValue(recordId);
    if (!query.exec() || query.numRowsAffected() != 1)
    {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

bool ControlRecordRepository::findAttempt(qint64 adminId, quint32 requestId, const QString &commandType,
                                          qint64 requestedPileId, qint64 requestedOrderId,
                                          const QString &requestScope, ControlAttemptRecord *record,
                                          QString *error) const
{
    *record = {};
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id,pile_id,order_id,result,detail FROM device_control_records "
                                 "WHERE admin_id=? AND request_id=? AND command_type=? "
                                 "ORDER BY CASE WHEN result='pending' THEN 1 ELSE 0 END,id DESC"));
    query.addBindValue(adminId);
    query.addBindValue(requestId);
    query.addBindValue(commandType);
    if (!query.exec())
    {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    while (query.next())
    {
        const auto detail = QJsonDocument::fromJson(query.value(4).toByteArray()).object();
        const auto request = detail.value("request").toObject();
        // Match the original request target, not a newly active order on the pile.
        if (request.value("_requestScope").toString() == requestScope &&
            request.value("pileId").toInteger() == requestedPileId &&
            request.value("orderId").toInteger() == requestedOrderId)
        {
            record->id = query.value(0).toLongLong();
            record->pileId = query.value(1).toLongLong();
            record->orderId = query.value(2).toLongLong();
            record->result = query.value(3).toString();
            record->detail = detail;
            break;
        }
    }
    return true;
}

bool ControlRecordRepository::applyPileCommand(
    qint64 pileId, const QString &command,
    const std::function<bool(const PileRecord &, QString *)> &persistResult, QString *reason,
    QString *error) const
{
    reason->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery begin(db);
    if (!begin.exec(QStringLiteral("BEGIN IMMEDIATE")))
    {
        if (error)
            *error = begin.lastError().text();
        return false;
    }
    auto fail = [&](const QString &message)
    {
        db.rollback();
        if (error)
            *error = message;
        return false;
    };
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("SELECT status,"
                       "EXISTS(SELECT 1 FROM charging_orders WHERE pile_id=? AND status='charging'),"
                       "EXISTS(SELECT 1 FROM reservations WHERE pile_id=? AND status='active') FROM "
                       "charging_piles WHERE id=?"));
    query.addBindValue(pileId);
    query.addBindValue(pileId);
    query.addBindValue(pileId);
    if (!query.exec())
        return fail(query.lastError().text());
    if (!query.next())
    {
        db.rollback();
        *reason = "not_found";
        return true;
    }
    const QString status = query.value(0).toString();
    const bool activeOrder = query.value(1).toBool();
    const bool activeReservation = query.value(2).toBool();
    query.finish();
    if (status == "charging" || activeOrder || status == "reserved" || activeReservation)
    {
        db.rollback();
        *reason = "command_conflict";
        return true;
    }
    QString target = status;
    if (command == "pile.disable")
        target = "disabled";
    else if (command == "pile.enable")
    {
        if (status != "disabled" && status != "idle")
        {
            db.rollback();
            *reason = "command_conflict";
            return true;
        }
        target = "idle";
    }
    else if (command == "pile.restart")
    {
        if (status == "offline" || status == "fault")
            target = "idle";
    }
    else
    {
        db.rollback();
        *reason = "command_conflict";
        return true;
    }
    query.prepare(
        QStringLiteral("UPDATE charging_piles SET status=?,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),"
                       "last_heartbeat_at=CASE WHEN ?='pile.restart' THEN "
                       "strftime('%Y-%m-%dT%H:%M:%SZ','now') ELSE last_heartbeat_at END WHERE id=?"));
    query.addBindValue(target);
    query.addBindValue(command);
    query.addBindValue(pileId);
    if (!query.exec() || query.numRowsAffected() != 1)
        return fail(query.lastError().text());
    PileRecord pile;
    QString resultError;
    if (!PileRepository(database()).findById(pileId, &pile, &resultError) ||
        !persistResult(pile, &resultError))
        return fail(resultError);
    if (!db.commit())
        return fail(db.lastError().text());
    return true;
}
