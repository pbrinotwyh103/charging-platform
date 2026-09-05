#include "repositories/controlrecordrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool ControlRecordRepository::insert(qint64 adminId, qint64 pileId, qint64 orderId,
                                     const QString &commandType, quint32 requestId,
                                     const QString &result, const QString &detail,
                                     qint64 *recordId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO device_control_records(admin_id,pile_id,order_id,command_type,"
        "request_id,result,detail) VALUES(?,?,?,?,?,?,?)"));
    query.addBindValue(adminId > 0 ? QVariant(adminId) : QVariant());
    query.addBindValue(pileId > 0 ? QVariant(pileId) : QVariant());
    query.addBindValue(orderId > 0 ? QVariant(orderId) : QVariant());
    query.addBindValue(commandType);
    query.addBindValue(requestId);
    query.addBindValue(result);
    query.addBindValue(detail);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (recordId) *recordId = query.lastInsertId().toLongLong();
    return true;
}
