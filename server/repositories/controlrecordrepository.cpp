#include "repositories/controlrecordrepository.h"

#include "database/databasemanager.h"

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

bool ControlRecordRepository::findSuccess(qint64 adminId, quint32 requestId, const QString &commandType,
                                          qint64 pileId, qint64 orderId, const QString &requestScope,
                                          QString *detail, QString *error) const
{
    detail->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen())
        return false;
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("SELECT detail FROM device_control_records WHERE admin_id=? AND request_id=? "
                       "AND command_type=? AND COALESCE(pile_id,0)=? AND (?=0 OR COALESCE(order_id,0)=?) AND "
                       "result='success' ORDER BY id DESC"));
    query.addBindValue(adminId);
    query.addBindValue(requestId);
    query.addBindValue(commandType);
    query.addBindValue(pileId);
    query.addBindValue(orderId);
    query.addBindValue(orderId);
    if (!query.exec())
    {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    while (query.next())
    {
        const auto candidate = query.value(0).toString();
        const auto request = QJsonDocument::fromJson(candidate.toUtf8()).object().value("request").toObject();
        if (request.value("_requestScope").toString() == requestScope)
        {
            *detail = candidate;
            break;
        }
    }
    return true;
}

bool ControlRecordRepository::applyPileCommand(qint64 pileId, const QString &command, QString *reason,
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
    if (status == "charging" || activeOrder ||
        (command != "pile.restart" && (status == "reserved" || activeReservation)))
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
    if (!db.commit())
        return fail(db.lastError().text());
    return true;
}
