#include "repositories/userrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

void readUser(QSqlQuery &query, UserRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->phone = query.value(1).toString();
    record->nickname = query.value(2).toString();
    record->avatarPath = query.value(3).toString();
    record->balanceCents = query.value(4).toLongLong();
    record->status = query.value(5).toString();
}

} // namespace

bool UserRepository::findByPhone(const QString &phone, UserRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_cents, status "
        "FROM users WHERE phone = ?"));
    query.addBindValue(phone);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readUser(query, record);
    return true;
}

bool UserRepository::findById(qint64 id, UserRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_cents, status "
        "FROM users WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readUser(query, record);
    return true;
}

bool UserRepository::findOrCreate(const QString &phone, UserRecord *record, bool *created,
                                  QString *error) const
{
    *created = false;
    if (!findByPhone(phone, record, error)) return false;
    if (record->id != 0) return true;

    QSqlDatabase db = database()->database(error);
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO users(phone, nickname, avatar_path, balance_cents, status) "
        "VALUES(?, ?, 'default://gray-avatar', 0, 'normal')"));
    insert.addBindValue(phone);
    insert.addBindValue(QStringLiteral("用户%1").arg(phone.right(4)));
    if (!insert.exec()) {
        db.rollback();
        if (error) *error = insert.lastError().text();
        return false;
    }
    *created = insert.numRowsAffected() == 1;
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return findByPhone(phone, record, error);
}
