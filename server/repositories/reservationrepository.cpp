#include "repositories/reservationrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {
void readReservation(QSqlQuery &query, ReservationRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->userId = query.value(1).toLongLong();
    record->pileId = query.value(2).toLongLong();
    record->status = query.value(3).toString();
    record->reservedAt = query.value(4).toString();
    record->expiresAt = query.value(5).toString();
    record->usedAt = query.value(6).toString();
}

bool rollback(QSqlDatabase &db, const QString &message, QString *error)
{
    db.rollback();
    if (error) *error = message;
    return false;
}
}

bool ReservationRepository::create(qint64 userId, qint64 pileId, const QString &expiresAt,
                                   qint64 *reservationId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    QSqlQuery user(db);
    user.prepare(QStringLiteral("SELECT status FROM users WHERE id=?"));
    user.addBindValue(userId);
    if (!user.exec() || !user.next()) {
        return rollback(db, user.lastError().isValid() ? user.lastError().text()
                                                       : QStringLiteral("用户不存在"), error);
    }
    if (user.value(0).toString() != QStringLiteral("normal")) {
        return rollback(db, QStringLiteral("冻结用户不能创建预约"), error);
    }

    QSqlQuery reservePile(db);
    reservePile.prepare(QStringLiteral(
        "UPDATE charging_piles SET status='reserved',updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='idle'"));
    reservePile.addBindValue(pileId);
    if (!reservePile.exec() || reservePile.numRowsAffected() != 1) {
        return rollback(db, reservePile.lastError().isValid() ? reservePile.lastError().text()
                : QStringLiteral("电桩不是空闲状态"), error);
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO reservations(user_id,pile_id,expires_at) VALUES(?,?,?)"));
    insert.addBindValue(userId);
    insert.addBindValue(pileId);
    insert.addBindValue(expiresAt);
    if (!insert.exec()) return rollback(db, insert.lastError().text(), error);
    const qint64 id = insert.lastInsertId().toLongLong();
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (reservationId) *reservationId = id;
    return true;
}

bool ReservationRepository::findActiveByUser(qint64 userId, ReservationRecord *record,
                                             QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id,user_id,pile_id,status,reserved_at,expires_at,used_at "
        "FROM reservations WHERE user_id=? AND status='active' ORDER BY id DESC LIMIT 1"));
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        record->id = 0;
        return true;
    }
    readReservation(query, record);
    return true;
}

bool ReservationRepository::cancel(qint64 reservationId, qint64 userId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery lookup(db);
    lookup.prepare(QStringLiteral(
        "SELECT pile_id FROM reservations WHERE id=? AND user_id=? AND status='active'"));
    lookup.addBindValue(reservationId);
    lookup.addBindValue(userId);
    if (!lookup.exec() || !lookup.next()) {
        return rollback(db, lookup.lastError().isValid() ? lookup.lastError().text()
                                                         : QStringLiteral("有效预约不存在"), error);
    }
    const qint64 pileId = lookup.value(0).toLongLong();
    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE reservations SET status='cancelled' WHERE id=?"));
    update.addBindValue(reservationId);
    if (!update.exec()) return rollback(db, update.lastError().text(), error);
    QSqlQuery release(db);
    release.prepare(QStringLiteral(
        "UPDATE charging_piles SET status='idle',updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='reserved'"));
    release.addBindValue(pileId);
    if (!release.exec()) return rollback(db, release.lastError().text(), error);
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

bool ReservationRepository::markUsed(qint64 reservationId, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE reservations SET status='used',used_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='active'"));
    query.addBindValue(reservationId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (error) *error = query.lastError().isValid()
            ? query.lastError().text() : QStringLiteral("有效预约不存在");
        return false;
    }
    return true;
}

bool ReservationRepository::expireDue(const QString &now, int *expiredCount,
                                      QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE reservations SET status='expired' WHERE status='active' AND expires_at<=?"));
    update.addBindValue(now);
    if (!update.exec()) return rollback(db, update.lastError().text(), error);
    const int count = update.numRowsAffected();
    QSqlQuery release(db);
    if (!release.exec(QStringLiteral(
        "UPDATE charging_piles SET status='idle',updated_at=CURRENT_TIMESTAMP "
        "WHERE status='reserved' AND NOT EXISTS(SELECT 1 FROM reservations r "
        "WHERE r.pile_id=charging_piles.id AND r.status='active')"))) {
        return rollback(db, release.lastError().text(), error);
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (expiredCount) *expiredCount = count;
    return true;
}
