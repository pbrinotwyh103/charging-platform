#include "repositories/pushrecordrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool PushRecordRepository::insert(const QString &targetRole, qint64 targetId,
                                  int messageType, quint32 requestId,
                                  const QString &result, qint64 *recordId,
                                  QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO push_records(target_role,target_id,message_type,request_id,result) "
        "VALUES(?,?,?,?,?)"));
    query.addBindValue(targetRole);
    query.addBindValue(targetId > 0 ? QVariant(targetId) : QVariant());
    query.addBindValue(messageType);
    query.addBindValue(requestId);
    query.addBindValue(result);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (recordId) *recordId = query.lastInsertId().toLongLong();
    return true;
}
