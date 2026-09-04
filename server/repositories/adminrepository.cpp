#include "repositories/adminrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

bool AdminRepository::findByUsername(const QString &username, AdminRecord *record,
                                     QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, username, password_hash, password_salt, permissions, status "
        "FROM admins WHERE username = ?"));
    query.addBindValue(username);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    record->id = query.value(0).toLongLong();
    record->username = query.value(1).toString();
    record->passwordHash = query.value(2).toString();
    record->passwordSalt = query.value(3).toString();
    record->permissions = query.value(4).toString();
    record->status = query.value(5).toString();
    return true;
}

bool AdminRepository::insert(const QString &username, const QString &passwordHash,
                             const QString &passwordSalt, const QString &permissions,
                             QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO admins(username, password_hash, password_salt, permissions) "
        "VALUES(?, ?, ?, ?)"));
    query.addBindValue(username);
    query.addBindValue(passwordHash);
    query.addBindValue(passwordSalt);
    query.addBindValue(permissions);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool AdminRepository::updateLastLogin(qint64 adminId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE admins SET last_login_at = CURRENT_TIMESTAMP WHERE id = ?"));
    query.addBindValue(adminId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}
