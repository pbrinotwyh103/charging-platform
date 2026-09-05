#include "repositories/walletrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {
bool rollback(QSqlDatabase &db, const QString &message, QString *error)
{
    db.rollback();
    if (error) *error = message;
    return false;
}

void readWallet(QSqlQuery &query, WalletRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->recordNo = query.value(1).toString();
    record->userId = query.value(2).toLongLong();
    record->orderId = query.value(3).toLongLong();
    record->recordType = query.value(4).toString();
    record->amountCents = query.value(5).toLongLong();
    record->balanceAfterCents = query.value(6).toLongLong();
    record->status = query.value(7).toString();
    record->createdAt = query.value(8).toString();
}
}

bool WalletRepository::recharge(const QString &recordNo, qint64 userId,
                                qint64 amountCents, qint64 *balanceAfterCents,
                                QString *error) const
{
    if (amountCents <= 0) {
        if (error) *error = QStringLiteral("充值金额必须大于0");
        return false;
    }
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE users SET balance_cents=balance_cents+?,updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='normal'"));
    update.addBindValue(amountCents);
    update.addBindValue(userId);
    if (!update.exec() || update.numRowsAffected() != 1) {
        return rollback(db, update.lastError().isValid() ? update.lastError().text()
                : QStringLiteral("用户不存在或已冻结"), error);
    }
    QSqlQuery balance(db);
    balance.prepare(QStringLiteral("SELECT balance_cents FROM users WHERE id=?"));
    balance.addBindValue(userId);
    if (!balance.exec() || !balance.next()) return rollback(db, balance.lastError().text(), error);
    const qint64 after = balance.value(0).toLongLong();
    QSqlQuery record(db);
    record.prepare(QStringLiteral(
        "INSERT INTO wallet_records(record_no,user_id,record_type,amount_cents,balance_after_cents) "
        "VALUES(?,?,'recharge',?,?)"));
    record.addBindValue(recordNo);
    record.addBindValue(userId);
    record.addBindValue(amountCents);
    record.addBindValue(after);
    if (!record.exec()) return rollback(db, record.lastError().text(), error);
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (balanceAfterCents) *balanceAfterCents = after;
    return true;
}

bool WalletRepository::settleOrder(const QString &recordNo, qint64 orderId,
                                   qint64 durationSeconds, qint64 energyWh,
                                   qint64 feeCents, const QString &finalStatus,
                                   const QString &stopReason,
                                   qint64 *balanceAfterCents, QString *error) const
{
    if (feeCents < 0 || (finalStatus != QStringLiteral("completed")
        && finalStatus != QStringLiteral("fault_stopped"))) {
        if (error) *error = QStringLiteral("无效的结算参数");
        return false;
    }
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery order(db);
    order.prepare(QStringLiteral(
        "SELECT user_id,pile_id FROM charging_orders WHERE id=? AND status='charging'"));
    order.addBindValue(orderId);
    if (!order.exec() || !order.next()) {
        return rollback(db, order.lastError().isValid() ? order.lastError().text()
                                                        : QStringLiteral("进行中的订单不存在"), error);
    }
    const qint64 userId = order.value(0).toLongLong();
    const qint64 pileId = order.value(1).toLongLong();
    QSqlQuery debit(db);
    debit.prepare(QStringLiteral(
        "UPDATE users SET balance_cents=balance_cents-?,updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND balance_cents>=?"));
    debit.addBindValue(feeCents);
    debit.addBindValue(userId);
    debit.addBindValue(feeCents);
    if (!debit.exec() || debit.numRowsAffected() != 1) {
        return rollback(db, debit.lastError().isValid() ? debit.lastError().text()
                                                        : QStringLiteral("钱包余额不足"), error);
    }
    QSqlQuery balance(db);
    balance.prepare(QStringLiteral("SELECT balance_cents FROM users WHERE id=?"));
    balance.addBindValue(userId);
    if (!balance.exec() || !balance.next()) return rollback(db, balance.lastError().text(), error);
    const qint64 after = balance.value(0).toLongLong();
    QSqlQuery wallet(db);
    wallet.prepare(QStringLiteral(
        "INSERT INTO wallet_records(record_no,user_id,order_id,record_type,amount_cents,"
        "balance_after_cents) VALUES(?,?,?,'charge_payment',?,?)"));
    wallet.addBindValue(recordNo);
    wallet.addBindValue(userId);
    wallet.addBindValue(orderId);
    wallet.addBindValue(-feeCents);
    wallet.addBindValue(after);
    if (!wallet.exec()) return rollback(db, wallet.lastError().text(), error);
    QSqlQuery finish(db);
    finish.prepare(QStringLiteral(
        "UPDATE charging_orders SET status=?,stopped_at=CURRENT_TIMESTAMP,duration_seconds=?,"
        "energy_wh=?,fee_cents=?,stop_reason=?,updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='charging'"));
    finish.addBindValue(finalStatus);
    finish.addBindValue(qMax<qint64>(0, durationSeconds));
    finish.addBindValue(qMax<qint64>(0, energyWh));
    finish.addBindValue(feeCents);
    finish.addBindValue(stopReason);
    finish.addBindValue(orderId);
    if (!finish.exec() || finish.numRowsAffected() != 1) {
        return rollback(db, finish.lastError().text(), error);
    }
    QSqlQuery release(db);
    release.prepare(QStringLiteral(
        "UPDATE charging_piles SET status='idle',total_charge_count=total_charge_count+1,"
        "total_charge_seconds=total_charge_seconds+?,updated_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='charging'"));
    release.addBindValue(qMax<qint64>(0, durationSeconds));
    release.addBindValue(pileId);
    if (!release.exec() || release.numRowsAffected() != 1) {
        return rollback(db, release.lastError().isValid() ? release.lastError().text()
                                                          : QStringLiteral("电桩状态异常"), error);
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (balanceAfterCents) *balanceAfterCents = after;
    return true;
}

bool WalletRepository::listByUser(qint64 userId, int limit, int offset,
                                  QList<WalletRecord> *records, QString *error) const
{
    records->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id,record_no,user_id,order_id,record_type,amount_cents,balance_after_cents,"
        "status,created_at FROM wallet_records WHERE user_id=? "
        "ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?"));
    query.addBindValue(userId);
    query.addBindValue(qBound(1, limit, 200));
    query.addBindValue(qMax(0, offset));
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        WalletRecord record;
        readWallet(query, &record);
        records->append(record);
    }
    return true;
}
