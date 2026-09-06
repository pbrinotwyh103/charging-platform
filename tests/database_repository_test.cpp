#include "database_repository_test.h"

#include "repositories/alarmrepository.h"
#include "repositories/controlrecordrepository.h"
#include "repositories/favoriterepository.h"
#include "repositories/orderrepository.h"
#include "repositories/pilerepository.h"
#include "repositories/pushrecordrepository.h"
#include "repositories/reservationrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"
#include "repositories/walletrepository.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

void DatabaseRepositoryTest::initTestCase()
{
    QVERIFY2(m_temporaryDirectory.isValid(), "无法创建临时测试目录");
    m_databasePath = m_temporaryDirectory.filePath(QStringLiteral("repository.db"));
    QString error;
    QVERIFY2(m_database.open(m_databasePath, &error), qPrintable(error));
}

qint64 DatabaseRepositoryTest::createUser(const QString &phone)
{
    UserRepository users(&m_database);
    UserRecord user;
    bool created = false;
    QString error;
    if (!users.findOrCreate(phone, &user, &created, &error)) {
        QTest::qFail(qPrintable(error), __FILE__, __LINE__);
        return 0;
    }
    return user.id;
}

void DatabaseRepositoryTest::schemaAndIntegrity()
{
    QString error;
    QCOMPARE(m_database.schemaVersion(&error), 3);
    QVERIFY2(m_database.checkIntegrity(&error), qPrintable(error));
    QSqlQuery query(m_database.database(&error));
    QVERIFY2(query.exec(QStringLiteral("PRAGMA foreign_keys")), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='index' "
        "AND name='idx_one_active_reservation_per_user'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void DatabaseRepositoryTest::profileStationAndFavoriteOperations()
{
    const qint64 userId = createUser(QStringLiteral("13800138001"));
    QVERIFY(userId > 0);
    QString error;
    UserRepository users(&m_database);
    QVERIFY2(users.updateProfile(userId, QStringLiteral("测试车主"),
                                 QStringLiteral("avatars/test.png"), &error), qPrintable(error));
    UserRecord user;
    QVERIFY(users.findById(userId, &user, &error));
    QCOMPARE(user.nickname, QStringLiteral("测试车主"));
    QCOMPARE(user.avatarPath, QStringLiteral("avatars/test.png"));

    StationRepository stations(&m_database);
    QList<StationRecord> stationList;
    QVERIFY2(stations.list(QStringLiteral("online"), &stationList, &error), qPrintable(error));
    QVERIFY(stationList.size() >= 2);
    QVERIFY(stationList.first().totalPileCount > 0);

    PileRepository piles(&m_database);
    QList<PileRecord> pileList;
    QVERIFY2(piles.listByStation(1, QString(), &pileList, &error), qPrintable(error));
    QCOMPARE(pileList.size(), 2);

    FavoriteRepository favorites(&m_database);
    bool changed = false;
    QVERIFY2(favorites.add(userId, 1, &changed, &error), qPrintable(error));
    QVERIFY(changed);
    QVERIFY(favorites.add(userId, 1, &changed, &error));
    QVERIFY(!changed);
    bool favorite = false;
    QVERIFY(favorites.contains(userId, 1, &favorite, &error));
    QVERIFY(favorite);
    QVERIFY(favorites.remove(userId, 1, &changed, &error));
    QVERIFY(changed);
}

void DatabaseRepositoryTest::reservationRulesAndExpiry()
{
    const qint64 firstUser = createUser(QStringLiteral("13800138002"));
    const qint64 secondUser = createUser(QStringLiteral("13800138003"));
    QString error;
    UserRepository users(&m_database);
    ReservationRepository reservations(&m_database);
    QVERIFY(users.setStatus(firstUser, QStringLiteral("frozen"), &error));
    qint64 reservationId = 0;
    QVERIFY(!reservations.create(firstUser, 1, QStringLiteral("2099-01-01 00:00:00"),
                                 &reservationId, &error));
    QVERIFY(users.setStatus(firstUser, QStringLiteral("normal"), &error));
    QVERIFY2(reservations.create(firstUser, 1, QStringLiteral("2099-01-01 00:00:00"),
                                 &reservationId, &error), qPrintable(error));
    QVERIFY(reservationId > 0);
    QVERIFY(!reservations.create(secondUser, 1, QStringLiteral("2099-01-01 00:00:00"),
                                 nullptr, &error));
    QVERIFY2(reservations.cancel(reservationId, firstUser, &error), qPrintable(error));

    QVERIFY2(reservations.create(firstUser, 2, QStringLiteral("2000-01-01 00:00:00"),
                                 &reservationId, &error), qPrintable(error));
    int expired = 0;
    QVERIFY2(reservations.expireDue(QStringLiteral("2026-09-05 00:00:00"),
                                    &expired, &error), qPrintable(error));
    QCOMPARE(expired, 1);
    PileRepository piles(&m_database);
    PileRecord pile;
    QVERIFY(piles.findById(2, &pile, &error));
    QCOMPARE(pile.status, QStringLiteral("idle"));
}

void DatabaseRepositoryTest::chargingSettlementAndRollback()
{
    const qint64 userId = createUser(QStringLiteral("13800138004"));
    QString error;
    WalletRepository wallet(&m_database);
    qint64 balance = 0;
    QVERIFY2(wallet.recharge(QStringLiteral("R-0001"), userId, 10000, &balance, &error),
             qPrintable(error));
    QCOMPARE(balance, 10000);

    OrderRepository orders(&m_database);
    qint64 orderId = 0;
    QVERIFY2(orders.createChargingOrder(QStringLiteral("O-0001"), userId, 1, 0,
                                        &orderId, &error), qPrintable(error));
    QVERIFY(orders.updateProgress(orderId, 120, 1000, 120, &error));
    QVERIFY2(wallet.settleOrder(QStringLiteral("P-0001"), orderId, 300, 2000, 250,
                                QStringLiteral("completed"), QStringLiteral("user_stop"),
                                &balance, &error), qPrintable(error));
    QCOMPARE(balance, 9750);
    OrderRecord completed;
    QVERIFY(orders.findById(orderId, &completed, &error));
    QCOMPARE(completed.status, QStringLiteral("completed"));
    QCOMPARE(completed.feeCents, 250);

    qint64 failingOrderId = 0;
    QVERIFY2(orders.createChargingOrder(QStringLiteral("O-0002"), userId, 2, 0,
                                        &failingOrderId, &error), qPrintable(error));
    QVERIFY(!wallet.settleOrder(QStringLiteral("P-0002"), failingOrderId, 60, 500, 99999,
                                QStringLiteral("completed"), QStringLiteral("user_stop"),
                                &balance, &error));
    OrderRecord stillCharging;
    QVERIFY(orders.findById(failingOrderId, &stillCharging, &error));
    QCOMPARE(stillCharging.status, QStringLiteral("charging"));
    UserRecord user;
    UserRepository users(&m_database);
    QVERIFY(users.findById(userId, &user, &error));
    QCOMPARE(user.balanceCents, 9750);
}

void DatabaseRepositoryTest::alarmControlAndPushRecords()
{
    QString error;
    AlarmRepository alarms(&m_database);
    AlarmRecord alarm;
    alarm.pileId = 2;
    alarm.alarmType = QStringLiteral("temperature_high");
    alarm.severity = QStringLiteral("critical");
    alarm.message = QStringLiteral("温度超过安全阈值");
    qint64 alarmId = 0;
    QVERIFY2(alarms.insert(alarm, &alarmId, &error), qPrintable(error));
    QVERIFY2(alarms.updateStatus(alarmId, QStringLiteral("resolved"), 0, &error),
             qPrintable(error));
    QList<AlarmRecord> records;
    QVERIFY(alarms.list(QStringLiteral("resolved"), 20, 0, &records, &error));
    QVERIFY(!records.isEmpty());

    ControlRecordRepository controls(&m_database);
    qint64 recordId = 0;
    QVERIFY2(controls.insert(0, 2, 0, QStringLiteral("restart"), 100,
                            QStringLiteral("success"), QString(), &recordId, &error),
             qPrintable(error));
    QVERIFY(recordId > 0);
    PushRecordRepository pushes(&m_database);
    QVERIFY2(pushes.insert(QStringLiteral("user"), createUser(QStringLiteral("13800138005")),
                           200, 101, QStringLiteral("success"), &recordId, &error),
             qPrintable(error));
}

void DatabaseRepositoryTest::backupAndRestore()
{
    const qint64 userId = createUser(QStringLiteral("13800138006"));
    QString error;
    UserRepository users(&m_database);
    QVERIFY(users.updateProfile(userId, QStringLiteral("备份前昵称"), QString(), &error));
    const QString backupPath = m_temporaryDirectory.filePath(QStringLiteral("backups/snapshot.db"));
    QVERIFY2(m_database.backupTo(backupPath, &error), qPrintable(error));
    QVERIFY(users.updateProfile(userId, QStringLiteral("备份后昵称"), QString(), &error));

    const QString corruptPath = m_temporaryDirectory.filePath(QStringLiteral("backups/corrupt.db"));
    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write("not-a-sqlite-database"), qint64(21));
    corrupt.close();
    QVERIFY(!m_database.restoreFrom(corruptPath, &error));

    QVERIFY2(m_database.restoreFrom(backupPath, &error), qPrintable(error));
    UserRecord restored;
    QVERIFY2(users.findById(userId, &restored, &error), qPrintable(error));
    QCOMPARE(restored.nickname, QStringLiteral("备份前昵称"));
    QVERIFY2(m_database.checkIntegrity(&error), qPrintable(error));
}

QTEST_GUILESS_MAIN(DatabaseRepositoryTest)
