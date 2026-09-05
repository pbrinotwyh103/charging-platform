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
    record->createdAt = query.value(6).toString();
    record->updatedAt = query.value(7).toString();
}

} // namespace

bool UserRepository::findByPhone(const QString &phone, UserRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_cents, status, created_at, updated_at "
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
        "SELECT id, phone, nickname, avatar_path, balance_cents, status, created_at, updated_at "
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

bool UserRepository::updateProfile(qint64 userId, const QString &nickname,
                                   const QString &avatarPath, QString *error) const
{
    if (nickname.trimmed().isEmpty() || nickname.size() > 40) {
        if (error) *error = QStringLiteral("昵称长度必须为1至40个字符");
        return false;
    }
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE users SET nickname=?, avatar_path=?, updated_at=CURRENT_TIMESTAMP WHERE id=?"));
    query.addBindValue(nickname.trimmed());
    query.addBindValue(avatarPath);
    query.addBindValue(userId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("用户不存在");
        return false;
    }
    return true;
}

bool UserRepository::setStatus(qint64 userId, const QString &status, QString *error) const
{
    if (status != QStringLiteral("normal") && status != QStringLiteral("frozen")) {
        if (error) *error = QStringLiteral("无效的用户状态");
        return false;
    }
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE users SET status=?, updated_at=CURRENT_TIMESTAMP WHERE id=?"));
    query.addBindValue(status);
    query.addBindValue(userId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("用户不存在");
        return false;
    }
    return true;
}

bool UserRepository::search(const QString &phoneKeyword, int limit, int offset,
                            QList<UserRecord> *records, QString *error) const
{
    records->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_cents, status, created_at, updated_at "
        "FROM users WHERE phone LIKE ? ORDER BY id DESC LIMIT ? OFFSET ?"));
    query.addBindValue(QStringLiteral("%") + phoneKeyword + QStringLiteral("%"));
    query.addBindValue(qBound(1, limit, 200));
    query.addBindValue(qMax(0, offset));
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        UserRecord record;
        readUser(query, &record);
        records->append(record);
    }
    return true;
}
