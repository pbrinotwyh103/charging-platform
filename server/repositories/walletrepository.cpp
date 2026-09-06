#include "repositories/walletrepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

bool rollback(QSqlDatabase &database, const QString &message, QString *error)
{
    database.rollback();
    if (error) *error = message;
    return false;
}

void readWallet(QSqlQuery &query, WalletRecord *record)
{
    record->id = query.value(0).toLongLong();
    record->recordNo = query.value(1).toString();
    record->userId = query.value(2).toLongLong();
    record->recordType = query.value(3).toString();
    record->amountCents = query.value(4).toLongLong();
    record->balanceAfterCents = query.value(5).toLongLong();
    record->status = query.value(6).toString();
    record->createdAt = query.value(7).toString();
}

} // namespace

bool WalletRepository::findByRecordNo(const QString &recordNo,
                                      const QString &recordType,
                                      WalletRecord *record,
                                      QString *error) const
{
    QSqlDatabase databaseConnection = database()->database(error);
    if (!databaseConnection.isValid() || !databaseConnection.isOpen())
        return false;
    QSqlQuery query(databaseConnection);
    query.prepare(QStringLiteral(
        "SELECT id,record_no,user_id,record_type,amount_cents,"
        "balance_after_cents,status,"
        "strftime('%Y-%m-%dT%H:%M:%SZ',created_at) "
        "FROM wallet_records WHERE record_no=? AND record_type=?"));
    query.addBindValue(recordNo);
    query.addBindValue(recordType);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    *record = WalletRecord();
    if (query.next()) readWallet(query, record);
    return true;
}

bool WalletRepository::recharge(const QString &recordNo, qint64 userId,
                                qint64 amountCents,
                                qint64 *balanceAfterCents,
                                QString *error) const
{
    QSqlDatabase databaseConnection = database()->database(error);
    if (!databaseConnection.isValid() || !databaseConnection.isOpen())
        return false;
    if (!databaseConnection.transaction()) {
        if (error) *error = databaseConnection.lastError().text();
        return false;
    }

    WalletRecord existing;
    if (!findByRecordNo(recordNo, QStringLiteral("recharge"),
                        &existing, error)) {
        return rollback(databaseConnection, error ? *error : QString(), error);
    }
    if (existing.id != 0) {
        if (existing.userId != userId || existing.amountCents != amountCents)
            return rollback(databaseConnection,
                            QStringLiteral("流水号已用于其他交易"), error);
        databaseConnection.rollback();
        if (balanceAfterCents)
            *balanceAfterCents = existing.balanceAfterCents;
        return true;
    }

    QSqlQuery update(databaseConnection);
    update.prepare(QStringLiteral(
        "UPDATE users SET balance_cents=balance_cents+?,"
        "updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') "
        "WHERE id=? AND status='normal'"));
    update.addBindValue(amountCents);
    update.addBindValue(userId);
    if (!update.exec() || update.numRowsAffected() != 1) {
        return rollback(databaseConnection,
                        update.lastError().isValid()
                            ? update.lastError().text()
                            : QStringLiteral("用户不存在或已冻结"), error);
    }

    QSqlQuery balance(databaseConnection);
    balance.prepare(QStringLiteral(
        "SELECT balance_cents FROM users WHERE id=?"));
    balance.addBindValue(userId);
    if (!balance.exec() || !balance.next())
        return rollback(databaseConnection, balance.lastError().text(), error);
    const qint64 updatedBalance = balance.value(0).toLongLong();

    QSqlQuery insert(databaseConnection);
    insert.prepare(QStringLiteral(
        "INSERT INTO wallet_records(record_no,user_id,record_type,"
        "amount_cents,balance_after_cents,status,created_at) "
        "VALUES(?,?,'recharge',?,?,'success',"
        "strftime('%Y-%m-%dT%H:%M:%SZ','now'))"));
    insert.addBindValue(recordNo);
    insert.addBindValue(userId);
    insert.addBindValue(amountCents);
    insert.addBindValue(updatedBalance);
    if (!insert.exec())
        return rollback(databaseConnection, insert.lastError().text(), error);
    if (!databaseConnection.commit()) {
        if (error) *error = databaseConnection.lastError().text();
        return false;
    }
    if (balanceAfterCents) *balanceAfterCents = updatedBalance;
    return true;
}

bool WalletRepository::countByUser(qint64 userId, int *total,
                                   QString *error) const
{
    QSqlDatabase databaseConnection = database()->database(error);
    if (!databaseConnection.isValid() || !databaseConnection.isOpen())
        return false;
    QSqlQuery query(databaseConnection);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM wallet_records WHERE user_id=?"));
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (total) *total = query.value(0).toInt();
    return true;
}

bool WalletRepository::listByUser(qint64 userId, int limit, int offset,
                                  QList<WalletRecord> *records,
                                  QString *error) const
{
    records->clear();
    QSqlDatabase databaseConnection = database()->database(error);
    if (!databaseConnection.isValid() || !databaseConnection.isOpen())
        return false;
    QSqlQuery query(databaseConnection);
    query.prepare(QStringLiteral(
        "SELECT id,record_no,user_id,record_type,amount_cents,"
        "balance_after_cents,status,"
        "strftime('%Y-%m-%dT%H:%M:%SZ',created_at) "
        "FROM wallet_records WHERE user_id=? "
        "ORDER BY julianday(created_at) DESC,id DESC LIMIT ? OFFSET ?"));
    query.addBindValue(userId);
    query.addBindValue(qBound(1, limit, 100));
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
