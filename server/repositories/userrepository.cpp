#include "repositories/userrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

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

bool UserRepository::count(const QString &phoneKeyword, int *total, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM users WHERE phone LIKE ?"));
    query.addBindValue(QStringLiteral("%") + phoneKeyword + QStringLiteral("%"));
    if (!query.exec() || !query.next()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (total) *total = query.value(0).toInt();
    return true;
}

bool UserRepository::findByPhone(const QString &phone, UserRecord *record, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_cents, status, "
        "strftime('%Y-%m-%dT%H:%M:%SZ',created_at),strftime('%Y-%m-%dT%H:%M:%SZ',updated_at) "
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
        "SELECT id, phone, nickname, avatar_path, balance_cents, status, "
        "strftime('%Y-%m-%dT%H:%M:%SZ',created_at),strftime('%Y-%m-%dT%H:%M:%SZ',updated_at) "
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
        "INSERT OR IGNORE INTO users(phone, nickname, avatar_path, balance_cents, status,created_at,updated_at) "
        "VALUES(?, ?, 'default://gray-avatar', 0, 'normal',"
        "strftime('%Y-%m-%dT%H:%M:%SZ','now'),strftime('%Y-%m-%dT%H:%M:%SZ','now'))"));
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
    return updateProfileFields(userId, nickname, avatarPath, error);
}

bool UserRepository::updateProfileFields(qint64 userId, const std::optional<QString> &nickname,
                                         const std::optional<QString> &avatarPath, QString *error) const
{
    if (!nickname && !avatarPath) {
        if (error) *error = QStringLiteral("未提供资料更新字段");
        return false;
    }
    if (nickname && (nickname->trimmed().isEmpty() || nickname->size() > 40)) {
        if (error) *error = QStringLiteral("昵称长度必须为1至40个字符");
        return false;
    }
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    // Omitted columns never enter the SET clause, so independent concurrent
    // patches cannot write back stale values read by the service.
    QStringList assignments;
    if (nickname) assignments.append(QStringLiteral("nickname=?"));
    if (avatarPath) assignments.append(QStringLiteral("avatar_path=?"));
    assignments.append(QStringLiteral("updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now')"));
    query.prepare(QStringLiteral("UPDATE users SET %1 WHERE id=?").arg(assignments.join(',')));
    if (nickname) query.addBindValue(nickname->trimmed());
    if (avatarPath) query.addBindValue(*avatarPath);
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
        "UPDATE users SET status=?, updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?"));
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
        "SELECT id, phone, nickname, avatar_path, balance_cents, status, "
        "strftime('%Y-%m-%dT%H:%M:%SZ',created_at),strftime('%Y-%m-%dT%H:%M:%SZ',updated_at) "
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
