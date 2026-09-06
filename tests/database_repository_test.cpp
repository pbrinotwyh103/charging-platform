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
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
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
    QCOMPARE(m_database.schemaVersion(&error), 5);
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

void DatabaseRepositoryTest::upgradeLegacyChargingSequence()
{
    const auto path = m_temporaryDirectory.filePath("legacy-sequence.db");
    const QString connection = "legacy-sequence-fixture";
    {
        auto legacy = QSqlDatabase::addDatabase("QSQLITE", connection);
        legacy.setDatabaseName(path);
        QVERIFY(legacy.open());
        QSqlQuery query(legacy);
        for (const auto &resource : {":/database/schema.sql", ":/database/seed.sql",
                                     ":/database/migrations/003_repository_indexes.sql"}) {
            QFile file(resource);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QString script;
            for (QString line : QString::fromUtf8(file.readAll()).split('\n')) {
                const int comment = line.indexOf("--");
                if (comment >= 0) line.truncate(comment);
                script += line + '\n';
            }
            for (const auto &statement : script.split(';', Qt::SkipEmptyParts)) {
                if (statement.trimmed().isEmpty()) continue;
                QVERIFY2(query.exec(statement), qPrintable(query.lastError().text()));
            }
        }
        QVERIFY(query.exec("INSERT INTO schema_version(version) VALUES(3)"));
        QVERIFY(query.exec("INSERT INTO users(id,phone,nickname,balance_cents) VALUES(1001,'13800138999','legacy',10000)"));
        QVERIFY(query.exec("UPDATE charging_piles SET status='charging' WHERE id=1"));
        QVERIFY(query.exec("INSERT INTO charging_orders(id,order_no,user_id,station_id,pile_id,status,started_at,duration_seconds,energy_wh,unit_price_cents,fee_cents) VALUES(1001,'legacy-sequence',1001,1,1,'charging','2026-09-06T00:00:00Z',60,1000,120,120)"));
        QVERIFY(query.exec("CREATE TRIGGER fail_version_four BEFORE INSERT ON schema_version WHEN NEW.version=4 BEGIN SELECT RAISE(ABORT,'migration interrupted'); END"));
    }
    QSqlDatabase::removeDatabase(connection);
    DatabaseManager upgraded;
    QString error;
    // The column and migration version must roll back together if recording fails.
    QVERIFY(!upgraded.open(path, &error));
    QSqlQuery query(upgraded.database());
    QVERIFY(query.exec("PRAGMA table_info(charging_orders)"));
    while (query.next()) QVERIFY(query.value(1).toString() != "push_seq");
    query.finish();
    QCOMPARE(upgraded.schemaVersion(&error), 3);
    QVERIFY(query.exec("DROP TRIGGER fail_version_four"));
    QVERIFY2(upgraded.initializeSchema(&error), qPrintable(error));
    QCOMPARE(upgraded.schemaVersion(&error), 5);
    QVERIFY(query.exec("SELECT push_seq,duration_seconds,energy_wh,fee_cents FROM charging_orders WHERE id=1001"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
    QCOMPARE(query.value(1).toInt(), 60);
    QCOMPARE(query.value(2).toInt(), 1000);
    QCOMPARE(query.value(3).toInt(), 120);
    query.finish();
    QVERIFY(query.exec("UPDATE charging_orders SET push_seq=9 WHERE id=1001"));
    QVERIFY(upgraded.initializeSchema(&error));
    QVERIFY(query.exec("SELECT push_seq FROM charging_orders WHERE id=1001"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 9);
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
    QVERIFY(WalletRepository(&m_database).recharge("R-RESERVATION-1", firstUser, 100, nullptr, &error));
    QVERIFY(WalletRepository(&m_database).recharge("R-RESERVATION-2", secondUser, 100, nullptr, &error));
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
    ReservationRecord cancelled;
    QVERIFY(reservations.findById(reservationId, &cancelled, &error));
    QCOMPARE(cancelled.status, QStringLiteral("cancelled"));
    PileRecord released;
    QVERIFY(PileRepository(&m_database).findById(1, &released, &error));
    QCOMPARE(released.status, QStringLiteral("idle"));

    QVERIFY2(reservations.create(firstUser, 2, QStringLiteral("2000-01-01 00:00:00"),
                                 &reservationId, &error), qPrintable(error));
    int expired = 0;
    QVERIFY2(reservations.expireDue(QStringLiteral("2026-09-05 00:00:00"),
                                    &expired, &error), qPrintable(error));
    QCOMPARE(expired, 1);
    QVERIFY(reservations.findById(reservationId, &cancelled, &error));
    QCOMPARE(cancelled.status, QStringLiteral("expired"));
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

void DatabaseRepositoryTest::rechargeIdempotency()
{
    const qint64 userId = createUser(QStringLiteral("13800138020"));
    const qint64 otherUser = createUser(QStringLiteral("13800138021"));
    WalletRepository wallet(&m_database);
    QString error;
    qint64 balance = 0;
    QVERIFY(wallet.recharge(QStringLiteral("R-IDEMPOTENT"), userId, 5000, &balance, &error));
    QVERIFY(wallet.recharge(QStringLiteral("R-LATER"), userId, 1000, &balance, &error));
    QVERIFY2(wallet.recharge(QStringLiteral("R-IDEMPOTENT"), userId, 5000, &balance, &error), qPrintable(error));
    QCOMPARE(balance, 5000);
    QVERIFY(!wallet.recharge(QStringLiteral("R-IDEMPOTENT"), otherUser, 5000, &balance, &error));
    QVERIFY(!wallet.recharge(QStringLiteral("R-IDEMPOTENT"), userId, 6000, &balance, &error));
    UserRecord user;
    QVERIFY(UserRepository(&m_database).findById(userId, &user, &error));
    QCOMPARE(user.balanceCents, 6000);
    WalletRecord record;
    QVERIFY(wallet.findByRecordNo(QStringLiteral("R-IDEMPOTENT"), "recharge", &record, &error));
    QCOMPARE(record.amountCents, 5000);
    QCOMPARE(record.balanceAfterCents, 5000);
    int count = -1;
    QVERIFY(wallet.countByUser(userId, &count, &error));
    QCOMPARE(count, 2);
    QVERIFY(wallet.findByRecordNo(QStringLiteral("missing"), "recharge", &record, &error));
    QCOMPARE(record.id, 0);
}

void DatabaseRepositoryTest::rechargeAndSettlementUseIndependentNamespaces()
{
    DatabaseManager database;
    QString error;
    QVERIFY(database.open(m_temporaryDirectory.filePath("wallet-namespaces.db"), &error));
    UserRecord user;
    bool created;
    QVERIFY(UserRepository(&database).findOrCreate("13800138776", &user, &created, &error));
    WalletRepository wallet(&database);
    qint64 balance, orderId;
    QVERIFY(wallet.recharge("ORDER-PAYMENT-1", user.id, 1000, &balance, &error));
    OrderRepository orders(&database);
    QVERIFY(orders.createChargingOrder("namespace-order", user.id, 1, 0, &orderId, &error));
    QCOMPARE(orderId, 1);
    QVERIFY2(orders.stopAndSettle(orderId, 60, 1000, 120, "completed", "user_stop", &balance, &error), qPrintable(error));
    QCOMPARE(balance, 880);
    QVERIFY(wallet.recharge("ORDER-PAYMENT-1", user.id, 1000, &balance, &error));
    QCOMPARE(balance, 1000);
    QVERIFY(orders.stopAndSettle(orderId, 90, 2000, 240, "completed", "user_stop", &balance, &error));
    QCOMPARE(balance, 880);
    QList<WalletRecord> records;
    QVERIFY(wallet.listByUser(user.id, 20, 0, &records, &error));
    QCOMPARE(records.size(), 2);
    QVERIFY(UserRepository(&database).findById(user.id, &user, &error));
    QCOMPARE(user.balanceCents, 880);
    // The type namespace must also protect a later recharge after settlement.
    QVERIFY(orders.createChargingOrder("namespace-order-2", user.id, 1, 0, &orderId, &error));
    QVERIFY(orders.stopAndSettle(orderId, 0, 0, 0, "completed", "user_stop", &balance, &error));
    QVERIFY(wallet.recharge("ORDER-PAYMENT-2", user.id, 100, &balance, &error));
    QCOMPARE(balance, 980);
    QVERIFY(wallet.recharge("ORDER-PAYMENT-2", user.id, 100, &balance, &error));
    QCOMPARE(balance, 980);
}

void DatabaseRepositoryTest::legacyWalletNamespaceMigration()
{
    const auto path = m_temporaryDirectory.filePath("legacy-wallet.db");
    const QString connection = "legacy-wallet-fixture";
    {
        auto legacy = QSqlDatabase::addDatabase("QSQLITE", connection);
        legacy.setDatabaseName(path);
        QVERIFY(legacy.open());
        QSqlQuery query(legacy);
        for (const auto &resource : {":/database/schema.sql", ":/database/seed.sql",
                 ":/database/migrations/003_repository_indexes.sql", ":/database/migrations/004_charging_push_sequence.sql"}) {
            QFile file(resource);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QString script;
            for (QString line : QString::fromUtf8(file.readAll()).split('\n')) {
                const int comment = line.indexOf("--");
                if (comment >= 0) line.truncate(comment);
                script += line + '\n';
            }
            for (const auto &statement : script.split(';', Qt::SkipEmptyParts)) {
                if (!statement.trimmed().isEmpty())
                    QVERIFY2(query.exec(statement), qPrintable(query.lastError().text()));
            }
        }
        QVERIFY(query.exec("INSERT INTO schema_version(version) VALUES(4)"));
        QVERIFY(query.exec("INSERT INTO users(id,phone,nickname,balance_cents) VALUES(1,'13800138775','legacy',1000)"));
        QVERIFY(query.exec("INSERT INTO wallet_records(id,record_no,user_id,record_type,amount_cents,balance_after_cents,created_at) VALUES(41,'ORDER-PAYMENT-1',1,'recharge',1000,1000,'2026-09-06T00:00:00Z')"));
        QVERIFY(query.exec("UPDATE sqlite_sequence SET seq=99 WHERE name='wallet_records'"));
        QVERIFY(query.exec("UPDATE charging_piles SET status='charging' WHERE id=1"));
        QVERIFY(query.exec("INSERT INTO charging_orders(id,order_no,user_id,station_id,pile_id,status,started_at,unit_price_cents) VALUES(1,'legacy-collision',1,1,1,'charging','2026-09-06T00:00:00Z',120)"));
        QVERIFY(query.exec("CREATE TRIGGER fail_version_five BEFORE INSERT ON schema_version WHEN NEW.version=5 BEGIN SELECT RAISE(ABORT,'migration interrupted'); END"));
    }
    QSqlDatabase::removeDatabase(connection);
    DatabaseManager database;
    QString error;
    QVERIFY(!database.open(path, &error));
    QCOMPARE(database.schemaVersion(&error), 4);
    QSqlQuery query(database.database());
    QVERIFY(query.exec("SELECT id,record_no,amount_cents FROM wallet_records"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 41);
    QCOMPARE(query.value(1).toString(), QString("ORDER-PAYMENT-1"));
    QCOMPARE(query.value(2).toInt(), 1000);
    query.finish();
    QVERIFY(!query.exec("INSERT INTO wallet_records(record_no,user_id,order_id,record_type,amount_cents,balance_after_cents) VALUES('ORDER-PAYMENT-1',1,1,'charge_payment',0,1000)"));
    QVERIFY(query.exec("DROP TRIGGER fail_version_five"));
    QVERIFY2(database.initializeSchema(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(&error), 5);
    qint64 balance;
    WalletRepository wallet(&database);
    QVERIFY(wallet.recharge("ORDER-PAYMENT-1", 1, 1000, &balance, &error));
    QCOMPARE(balance, 1000);
    OrderRepository orders(&database);
    QVERIFY2(orders.stopAndSettle(1, 60, 1000, 120, "completed", "user_stop", &balance, &error), qPrintable(error));
    QCOMPARE(balance, 880);
    QVERIFY(database.initializeSchema(&error));
    QVERIFY(wallet.recharge("ORDER-PAYMENT-1", 1, 1000, &balance, &error));
    QCOMPARE(balance, 1000);
    QVERIFY(orders.stopAndSettle(1, 99, 2000, 999, "completed", "user_stop", &balance, &error));
    QCOMPARE(balance, 880);
    QVERIFY(database.checkIntegrity(&error));
    QList<WalletRecord> records;
    QVERIFY(wallet.listByUser(1, 20, 0, &records, &error));
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.first().id, 100);
    QCOMPARE(records.last().id, 41);
    QCOMPARE(records.last().createdAt, QString("2026-09-06T00:00:00Z"));
}

void DatabaseRepositoryTest::atomicSettlementAndRecovery()
{
    const qint64 userId = createUser(QStringLiteral("13800138022"));
    QString error;
    WalletRepository wallet(&m_database);
    OrderRepository orders(&m_database);
    PileRepository piles(&m_database);
    qint64 balance = 0;
    QVERIFY(wallet.recharge(QStringLiteral("R-ATOMIC"), userId, 1000, &balance, &error));
    PileRecord before;
    QVERIFY(piles.findById(1, &before, &error));
    qint64 orderId = 0;
    QVERIFY(orders.createChargingOrder(QStringLiteral("O-ATOMIC"), userId, 1, 0, &orderId, &error));
    qint64 duplicateId = 0;
    QVERIFY(orders.createChargingOrder(QStringLiteral("O-ATOMIC"), userId, 1, 0, &duplicateId, &error));
    QCOMPARE(duplicateId, orderId);
    QVERIFY(orders.updateProgress(orderId, 10, 100, 12, &error));

    // A fresh manager models service restart without relying on runtime memory.
    DatabaseManager reopened;
    QVERIFY(reopened.open(m_databasePath, &error));
    QList<OrderRecord> active;
    QVERIFY(OrderRepository(&reopened).listActive(&active, &error));
    bool recovered = false;
    for (const OrderRecord &record : active) {
        if (record.id == orderId) {
            recovered = true;
            QCOMPARE(record.durationSeconds, 10);
            QCOMPARE(record.energyWh, 100);
        }
    }
    QVERIFY(recovered);

    QVERIFY(!orders.stopAndSettle(orderId, 60, 500, 1001, QStringLiteral("completed"),
                                  QStringLiteral("user_stop"), &balance, &error));
    OrderRecord order;
    QVERIFY(orders.findById(orderId, &order, &error));
    QCOMPARE(order.status, QStringLiteral("charging"));
    QCOMPARE(order.durationSeconds, 10);
    QCOMPARE(order.feeCents, 12);
    int count = 0;
    QVERIFY(wallet.countByUser(userId, &count, &error));
    QCOMPARE(count, 1);

    // Force the last write to fail: all preceding debit/ledger/order writes must roll back.
    QSqlQuery inject(m_database.database(&error));
    QVERIFY(inject.exec(QStringLiteral("CREATE TEMP TRIGGER fail_release BEFORE UPDATE ON charging_piles "
        "WHEN OLD.id=1 AND NEW.status='idle' BEGIN SELECT RAISE(ABORT,'injected release failure'); END")));
    QVERIFY(!orders.stopAndSettle(orderId, 60, 500, 250, QStringLiteral("completed"),
                                  QStringLiteral("user_stop"), &balance, &error));
    QVERIFY(inject.exec(QStringLiteral("DROP TRIGGER fail_release")));
    QVERIFY(orders.findById(orderId, &order, &error));
    QCOMPARE(order.status, QStringLiteral("charging"));
    QCOMPARE(order.energyWh, 100);
    UserRecord user;
    QVERIFY(UserRepository(&m_database).findById(userId, &user, &error));
    QCOMPARE(user.balanceCents, 1000);
    QVERIFY(wallet.countByUser(userId, &count, &error));
    QCOMPARE(count, 1);
    PileRecord pile;
    QVERIFY(piles.findById(1, &pile, &error));
    QCOMPARE(pile.status, QStringLiteral("charging"));
    QCOMPARE(pile.totalChargeCount, before.totalChargeCount);
    QCOMPARE(pile.totalChargeSeconds, before.totalChargeSeconds);

    QVERIFY2(orders.stopAndSettle(orderId, 60, 500, 250, QStringLiteral("completed"),
                                 QStringLiteral("user_stop"), &balance, &error), qPrintable(error));
    QCOMPARE(balance, 750);
    QVERIFY(wallet.recharge(QStringLiteral("R-AFTER-STOP"), userId, 100, &balance, &error));
    QVERIFY(orders.stopAndSettle(orderId, 90, 900, 400, QStringLiteral("fault_stopped"),
                                QStringLiteral("retry"), &balance, &error));
    QCOMPARE(balance, 750);
    QVERIFY(orders.findById(orderId, &order, &error));
    QCOMPARE(order.status, QStringLiteral("completed"));
    QCOMPARE(order.durationSeconds, 60);
    QCOMPARE(order.energyWh, 500);
    QCOMPARE(order.feeCents, 250);
    QCOMPARE(order.stopReason, QStringLiteral("user_stop"));
    QVERIFY(!order.stoppedAt.isEmpty());
    QVERIFY(piles.findById(1, &pile, &error));
    QCOMPARE(pile.status, QStringLiteral("idle"));
    QCOMPARE(pile.totalChargeCount, before.totalChargeCount + 1);
    QCOMPARE(pile.totalChargeSeconds, before.totalChargeSeconds + 60);
    QList<WalletRecord> ledger;
    QVERIFY(wallet.listByUser(userId, 20, 0, &ledger, &error));
    QCOMPARE(ledger.size(), 3);
    QCOMPARE(ledger.at(1).amountCents, -250);
    QCOMPARE(ledger.at(1).orderId, orderId);
    QVERIFY(orders.listActive(&active, &error));
    for (const OrderRecord &record : active) QVERIFY(record.id != orderId);
}

void DatabaseRepositoryTest::repositoryPagination()
{
    QString error;
    int count = 0;
    UserRepository users(&m_database);
    QList<UserRecord> userList;
    QVERIFY(users.count(QStringLiteral("1380013802"), &count, &error));
    QCOMPARE(count, 3);
    QVERIFY(users.search(QStringLiteral("1380013802"), 1, 1, &userList, &error));
    QCOMPARE(userList.size(), 1);
    QCOMPARE(userList.first().phone, QStringLiteral("13800138021"));
    StationRepository stations(&m_database);
    QList<StationRecord> allStations, stationPage;
    QVERIFY(stations.list(QStringLiteral("online"), &allStations, &error));
    QVERIFY(stations.count(QStringLiteral("online"), &count, &error));
    QCOMPARE(count, allStations.size());
    QVERIFY(stations.list(QStringLiteral("online"), 1, 1, &stationPage, &error));
    QCOMPARE(stationPage.size(), 1);
    QCOMPARE(stationPage.first().id, allStations.at(1).id);
    PileRepository piles(&m_database);
    QList<PileRecord> pilePage;
    QVERIFY(piles.countByStation(1, QStringLiteral("idle"), &count, &error));
    QCOMPARE(count, 1);
    QVERIFY(piles.listByStation(1, QStringLiteral("idle"), 1, 0, &pilePage, &error));
    QCOMPARE(pilePage.size(), 1);
    QCOMPARE(pilePage.first().id, 1);
    OrderRepository orders(&m_database);
    QList<OrderRecord> orderPage;
    QVERIFY(orders.count(QStringLiteral("completed"), &count, &error));
    QCOMPARE(count, 2);
    QVERIFY(orders.list(QStringLiteral("completed"), 1, 0, &orderPage, &error));
    QCOMPARE(orderPage.size(), 1);
    QCOMPARE(orderPage.first().orderNo, QStringLiteral("O-ATOMIC"));
    QVERIFY(orders.countByUser(orderPage.first().userId, &count, &error));
    QCOMPARE(count, 1);
    QVERIFY(orders.list(QStringLiteral("completed"), 1, 2, &orderPage, &error));
    QVERIFY(orderPage.isEmpty());
}

void DatabaseRepositoryTest::faultSettlementAndLegacyReplay()
{
    const qint64 userId = createUser(QStringLiteral("13800138023"));
    QString error;
    WalletRepository wallet(&m_database);
    OrderRepository orders(&m_database);
    PileRepository piles(&m_database);
    qint64 balance = 0, orderId = 0;
    QVERIFY(wallet.recharge(QStringLiteral("R-FAULT"), userId, 1000, &balance, &error));
    QVERIFY(orders.createChargingOrder(QStringLiteral("O-FAULT"), userId, 1, 0, &orderId, &error));
    QVERIFY(!orders.createChargingOrder(QStringLiteral("O-FAULT"), userId, 2, 0, nullptr, &error));
    QVERIFY(piles.updateStatus(1, QStringLiteral("charging"), QStringLiteral("fault"), &error));
    QVERIFY2(wallet.settleOrder(QStringLiteral("P-FAULT"), orderId, 30, 100, 100,
                                QStringLiteral("fault_stopped"), QStringLiteral("device_fault"),
                                &balance, &error), qPrintable(error));
    QCOMPARE(balance, 900);
    QVERIFY(orders.stopAndSettle(orderId, 30, 100, 100, QStringLiteral("fault_stopped"),
                                QStringLiteral("device_fault"), &balance, &error));
    QCOMPARE(balance, 900);
    int count = 0;
    QVERIFY(wallet.countByUser(userId, &count, &error));
    QCOMPARE(count, 2);
    PileRecord pile;
    QVERIFY(piles.findById(1, &pile, &error));
    QCOMPARE(pile.status, QStringLiteral("idle"));
}

qint64 DatabaseRepositoryTest::insertLegacyReservation(qint64 userId, const QString &expiresAt)
{
    QSqlQuery fixture(m_database.database());
    fixture.prepare(QStringLiteral("INSERT INTO reservations(user_id,pile_id,reserved_at,expires_at) "
        "VALUES(?,1,'2026-09-06 04:05:06',?)"));
    fixture.addBindValue(userId);
    fixture.addBindValue(expiresAt);
    if (!fixture.exec()) {
        QTest::qFail(qPrintable(fixture.lastError().text()), __FILE__, __LINE__);
        return 0;
    }
    const qint64 id = fixture.lastInsertId().toLongLong();
    if (!fixture.exec(QStringLiteral("UPDATE charging_piles SET status='reserved',"
        "updated_at='2026-09-06 04:05:06' WHERE id=1"))) {
        QTest::qFail(qPrintable(fixture.lastError().text()), __FILE__, __LINE__);
        return 0;
    }
    return id;
}

void DatabaseRepositoryTest::mixedFormatReservationExpiry()
{
    const qint64 userId = createUser(QStringLiteral("13800138024"));
    ReservationRepository reservations(&m_database);
    QString error;
    qint64 reservationId = 0;
    int expired = 0;
    struct Case { const char *expiry; const char *now; int expected; };
    const Case cases[] = {
        {"2098-06-01T09:00:00Z", "2098-06-01 10:00:00", 1},
        {"2098-06-01 11:00:00", "2098-06-01T10:00:00Z", 0},
        {"2098-06-01T18:00:00+08:00", "2098-06-01 10:00:00", 1},
        {"2098-06-01 10:00:00", "2098-06-01T10:00:00Z", 1}
    };
    for (const Case &entry : cases) {
        reservationId = insertLegacyReservation(userId, QString::fromLatin1(entry.expiry));
        QVERIFY(reservationId > 0);
        QVERIFY(reservations.expireDue(QString::fromLatin1(entry.now), &expired, &error));
        QCOMPARE(expired, entry.expected);
        ReservationRecord reservation;
        QVERIFY(reservations.findById(reservationId, &reservation, &error));
        QCOMPARE(reservation.status, entry.expected ? QStringLiteral("expired") : QStringLiteral("active"));
        PileRecord pile;
        QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
        QCOMPARE(pile.status, entry.expected ? QStringLiteral("idle") : QStringLiteral("reserved"));
        if (!entry.expected) QVERIFY(reservations.cancel(reservationId, userId, &error));
    }
}

void DatabaseRepositoryTest::isoExpiredReservationCannotStart()
{
    const qint64 userId = createUser(QStringLiteral("13800138025"));
    ReservationRepository reservations(&m_database);
    OrderRepository orders(&m_database);
    QString error;
    qint64 reservationId = 0, orderId = 0;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(WalletRepository(&m_database).recharge("R-EXPIRY", userId, 100, nullptr, &error));
    QVERIFY(reservations.create(userId, 1, now.addSecs(-1).toString(Qt::ISODate), &reservationId, &error));
    QVERIFY(!orders.createChargingOrder(QStringLiteral("O-ISO-EXPIRED"), userId, 1,
                                        reservationId, &orderId, &error));
    ReservationRecord reservation;
    QVERIFY(reservations.findById(reservationId, &reservation, &error));
    QCOMPARE(reservation.status, QStringLiteral("active"));
    QVERIFY(reservations.cancel(reservationId, userId, &error));
    // Legacy SQL timestamps remain valid for a future reservation.
    reservationId = insertLegacyReservation(userId, now.addSecs(600).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    QVERIFY(reservationId > 0);
    QVERIFY(orders.createChargingOrder(QStringLiteral("O-LEGACY-FUTURE"), userId, 1,
                                       reservationId, &orderId, &error));
    QVERIFY(orders.stopAndSettle(orderId, 0, 0, 0, QStringLiteral("completed"),
                                QStringLiteral("test_stop"), nullptr, &error));
}

void DatabaseRepositoryTest::repositoryUtcTimestamps()
{
    QString error;
    QSqlQuery fixture(m_database.database(&error));
    QVERIFY(fixture.exec(QStringLiteral("INSERT INTO users(phone,nickname,balance_cents,created_at,updated_at) "
        "VALUES('13800138026','legacy',100,'2026-09-06 04:05:06','2026-09-06T12:05:06+08:00')")));
    const qint64 userId = fixture.lastInsertId().toLongLong();
    UserRecord user;
    QVERIFY(UserRepository(&m_database).findById(userId, &user, &error));
    QCOMPARE(user.createdAt, QStringLiteral("2026-09-06T04:05:06Z"));
    QCOMPARE(user.updatedAt, QStringLiteral("2026-09-06T04:05:06Z"));

    auto isUtc = [](const QString &value) {
        const QDateTime parsed = QDateTime::fromString(value, Qt::ISODate);
        return value.endsWith(QLatin1Char('Z')) && value.contains(QLatin1Char('T'))
            && parsed.isValid() && parsed.offsetFromUtc() == 0;
    };
    StationRecord station;
    QVERIFY(StationRepository(&m_database).findById(1, &station, &error));
    QVERIFY(isUtc(station.createdAt));
    QVERIFY(isUtc(station.updatedAt));
    PileRepository piles(&m_database);
    QVERIFY(piles.updateHeartbeat(1, &error));
    PileRecord pile;
    QVERIFY(piles.findById(1, &pile, &error));
    QVERIFY(isUtc(pile.lastHeartbeatAt));
    QVERIFY(isUtc(pile.updatedAt));

    ReservationRepository reservations(&m_database);
    qint64 reservationId = 0;
    QVERIFY(reservations.create(userId, 1, QStringLiteral("2099-01-01 12:00:00"), &reservationId, &error));
    ReservationRecord reservation;
    QVERIFY(reservations.findById(reservationId, &reservation, &error));
    QCOMPARE(reservation.expiresAt, QStringLiteral("2099-01-01T12:00:00Z"));
    QVERIFY(isUtc(reservation.reservedAt));
    QVERIFY(reservation.usedAt.isEmpty());
    OrderRepository orders(&m_database);
    qint64 orderId = 0;
    QVERIFY(orders.createChargingOrder(QStringLiteral("O-UTC"), userId, 1, reservationId, &orderId, &error));
    QVERIFY(reservations.findById(reservationId, &reservation, &error));
    QVERIFY(isUtc(reservation.usedAt));
    QVERIFY(orders.stopAndSettle(orderId, 0, 0, 0, QStringLiteral("completed"),
                                QStringLiteral("test_stop"), nullptr, &error));
    OrderRecord order;
    QVERIFY(orders.findById(orderId, &order, &error));
    QVERIFY(isUtc(order.startedAt));
    QVERIFY(isUtc(order.stoppedAt));
    QVERIFY(isUtc(order.createdAt));
    QVERIFY(isUtc(order.updatedAt));
    QList<WalletRecord> ledger;
    QVERIFY(WalletRepository(&m_database).listByUser(userId, 20, 0, &ledger, &error));
    QCOMPARE(ledger.size(), 1);
    QVERIFY(isUtc(ledger.first().createdAt));
    fixture.prepare(QStringLiteral("SELECT o.stopped_at,o.updated_at,w.created_at,u.updated_at,p.updated_at "
        "FROM charging_orders o JOIN wallet_records w ON w.order_id=o.id "
        "JOIN users u ON u.id=o.user_id JOIN charging_piles p ON p.id=o.pile_id WHERE o.id=?"));
    fixture.addBindValue(orderId);
    QVERIFY(fixture.exec());
    QVERIFY(fixture.next());
    for (int column = 0; column < 5; ++column) QVERIFY(isUtc(fixture.value(column).toString()));
}

void DatabaseRepositoryTest::mixedFormatPagination()
{
    const qint64 userId = createUser(QStringLiteral("13800138027"));
    OrderRepository orders(&m_database);
    WalletRepository wallet(&m_database);
    QString error;
    qint64 ids[2] = {};
    const QString dates[] = {QStringLiteral("2098-06-01 11:00:00"), QStringLiteral("2098-06-01T09:00:00Z")};
    for (int index = 0; index < 2; ++index) {
        QSqlQuery fixture(m_database.database(&error));
        fixture.prepare(QStringLiteral("INSERT INTO charging_orders(order_no,user_id,station_id,pile_id,status,"
            "started_at,unit_price_cents,created_at) VALUES(?,?,1,1,'completed','2098-06-01 08:00:00',100,?)"));
        fixture.addBindValue(QStringLiteral("O-TIME-PAGE-%1").arg(index));
        fixture.addBindValue(userId);
        fixture.addBindValue(dates[index]);
        QVERIFY(fixture.exec());
        ids[index] = fixture.lastInsertId().toLongLong();
        fixture.prepare(QStringLiteral("INSERT INTO wallet_records(record_no,user_id,order_id,record_type,"
            "amount_cents,balance_after_cents,created_at) VALUES(?,?,?,'charge_payment',0,0,?)"));
        fixture.addBindValue(QStringLiteral("W-TIME-PAGE-%1").arg(index));
        fixture.addBindValue(userId);
        fixture.addBindValue(ids[index]);
        fixture.addBindValue(dates[index]);
        QVERIFY(fixture.exec());
    }
    QList<OrderRecord> page;
    QVERIFY(orders.listByUser(userId, 1, 0, &page, &error));
    QCOMPARE(page.size(), 1);
    QCOMPARE(page.first().id, ids[0]);
    QVERIFY(orders.list(QStringLiteral("completed"), 1, 0, &page, &error));
    QCOMPARE(page.size(), 1);
    QCOMPARE(page.first().id, ids[0]);
    QList<WalletRecord> ledger;
    QVERIFY(wallet.listByUser(userId, 1, 0, &ledger, &error));
    QCOMPARE(ledger.size(), 1);
    QCOMPARE(ledger.first().orderId, ids[0]);
}

void DatabaseRepositoryTest::freshTimestampWrites_data()
{
    QTest::addColumn<QString>("operation");
    const QStringList operations = {
        "user_create", "user_profile", "user_status", "station_create", "station_update",
        "pile_create", "pile_status", "pile_heartbeat", "wallet_recharge",
        "reservation_create", "reservation_cancel", "reservation_expire", "reservation_use",
        "order_create", "order_reserved_start", "order_progress", "order_settle"
    };
    for (const QString &operation : operations) QTest::newRow(qPrintable(operation)) << operation;
}

void DatabaseRepositoryTest::freshTimestampWrites()
{
    QFETCH(QString, operation);
    const QDateTime earliest = QDateTime::currentDateTimeUtc().addSecs(-1);
    QString error;
    QSqlQuery raw(m_database.database(&error));
    // Setup is deliberately legacy SQL data so each row checks its own write path.
    raw.prepare(QStringLiteral("INSERT INTO users(phone,nickname,balance_cents,created_at,updated_at) "
        "VALUES(?,?,1000,'2026-09-06 04:05:06','2026-09-06 04:05:06')"));
    raw.addBindValue(QStringLiteral("fixture-%1").arg(operation));
    raw.addBindValue(QStringLiteral("fixture"));
    QVERIFY(raw.exec());
    qint64 userId = raw.lastInsertId().toLongLong();
    QVERIFY(raw.exec(QStringLiteral("INSERT INTO stations(name,address,longitude,latitude,price_cents_per_kwh,"
        "created_at,updated_at) VALUES('fixture','fixture',1,1,100,'2026-09-06 04:05:06','2026-09-06 04:05:06')")));
    qint64 stationId = raw.lastInsertId().toLongLong();
    raw.prepare(QStringLiteral("INSERT INTO charging_piles(station_id,pile_code,charge_type,power_kw,updated_at) "
        "VALUES(?,?,'fast',60,'2026-09-06 04:05:06')"));
    raw.addBindValue(stationId);
    raw.addBindValue(operation);
    QVERIFY(raw.exec());
    qint64 pileId = raw.lastInsertId().toLongLong();
    UserRepository users(&m_database);
    StationRepository stations(&m_database);
    PileRepository piles(&m_database);
    WalletRepository wallet(&m_database);
    ReservationRepository reservations(&m_database);
    OrderRepository orders(&m_database);
    QString sql;
    qint64 targetId = 0;

    if (operation.startsWith(QStringLiteral("user_"))) {
        if (operation == QStringLiteral("user_create")) {
            UserRecord user;
            bool created = false;
            QVERIFY(users.findOrCreate(QStringLiteral("fresh-utc-user"), &user, &created, &error));
            QVERIFY(created);
            userId = user.id;
            sql = QStringLiteral("SELECT created_at,updated_at FROM users WHERE id=?");
        } else {
            if (operation == QStringLiteral("user_profile"))
                QVERIFY(users.updateProfile(userId, QStringLiteral("updated"), QStringLiteral("avatar"), &error));
            else QVERIFY(users.setStatus(userId, QStringLiteral("frozen"), &error));
            sql = QStringLiteral("SELECT updated_at FROM users WHERE id=?");
        }
        targetId = userId;
    } else if (operation.startsWith(QStringLiteral("station_"))) {
        StationRecord station;
        QVERIFY(stations.findById(stationId, &station, &error));
        if (operation == QStringLiteral("station_create")) {
            QVERIFY(stations.insert(station, &stationId, &error));
            sql = QStringLiteral("SELECT created_at,updated_at FROM stations WHERE id=?");
        } else {
            QVERIFY(stations.update(station, &error));
            sql = QStringLiteral("SELECT updated_at FROM stations WHERE id=?");
        }
        targetId = stationId;
    } else if (operation.startsWith(QStringLiteral("pile_"))) {
        if (operation == QStringLiteral("pile_create")) {
            PileRecord pile;
            QVERIFY(piles.findById(pileId, &pile, &error));
            pile.pileCode = QStringLiteral("fresh-utc-pile");
            QVERIFY(piles.insert(pile, &pileId, &error));
            sql = QStringLiteral("SELECT updated_at FROM charging_piles WHERE id=?");
        } else if (operation == QStringLiteral("pile_heartbeat")) {
            QVERIFY(piles.updateHeartbeat(pileId, &error));
            sql = QStringLiteral("SELECT last_heartbeat_at,updated_at FROM charging_piles WHERE id=?");
        } else {
            QVERIFY(piles.updateStatus(pileId, QStringLiteral("idle"), QStringLiteral("offline"), &error));
            sql = QStringLiteral("SELECT updated_at FROM charging_piles WHERE id=?");
        }
        targetId = pileId;
    } else if (operation == QStringLiteral("wallet_recharge")) {
        QVERIFY(wallet.recharge(QStringLiteral("R-RAW-UTC"), userId, 100, nullptr, &error));
        sql = QStringLiteral("SELECT w.created_at,u.updated_at FROM wallet_records w "
            "JOIN users u ON u.id=w.user_id WHERE w.user_id=?");
        targetId = userId;
    } else {
        qint64 reservationId = 0, orderId = 0;
        if (operation.startsWith(QStringLiteral("reservation_")) || operation == QStringLiteral("order_reserved_start")) {
            if (operation == QStringLiteral("reservation_create")) {
                QVERIFY(reservations.create(userId, pileId, QStringLiteral("2099-01-01T20:00:00+08:00"),
                                            &reservationId, &error));
                raw.prepare(QStringLiteral("SELECT expires_at FROM reservations WHERE id=?"));
                raw.addBindValue(reservationId);
                QVERIFY(raw.exec()); QVERIFY(raw.next());
                QCOMPARE(raw.value(0).toString(), QStringLiteral("2099-01-01T12:00:00Z"));
                sql = QStringLiteral("SELECT r.reserved_at,r.expires_at,p.updated_at FROM reservations r "
                    "JOIN charging_piles p ON p.id=r.pile_id WHERE r.id=?");
            } else {
                raw.prepare(QStringLiteral("INSERT INTO reservations(user_id,pile_id,reserved_at,expires_at) "
                    "VALUES(?,?,'2026-09-06 04:05:06','2099-01-01 12:00:00')"));
                raw.addBindValue(userId); raw.addBindValue(pileId);
                QVERIFY(raw.exec());
                reservationId = raw.lastInsertId().toLongLong();
                raw.prepare(QStringLiteral("UPDATE charging_piles SET status='reserved' WHERE id=?"));
                raw.addBindValue(pileId); QVERIFY(raw.exec());
                if (operation == QStringLiteral("reservation_cancel")) {
                    QVERIFY(reservations.cancel(reservationId, userId, &error));
                } else if (operation == QStringLiteral("reservation_expire")) {
                    int expired = 0;
                    QVERIFY(reservations.expireDue(QStringLiteral("2100-01-01T00:00:00Z"), &expired, &error));
                    QVERIFY(expired > 0);
                } else if (operation == QStringLiteral("reservation_use")) {
                    QVERIFY(reservations.markUsed(reservationId, &error));
                }
                sql = operation == QStringLiteral("reservation_use")
                    ? QStringLiteral("SELECT used_at FROM reservations WHERE id=?")
                    : QStringLiteral("SELECT p.updated_at FROM reservations r JOIN charging_piles p ON p.id=r.pile_id WHERE r.id=?");
            }
            targetId = reservationId;
        }
        if (operation.startsWith(QStringLiteral("order_"))) {
            QVERIFY(orders.createChargingOrder(operation, userId, pileId, reservationId, &orderId, &error));
            if (operation == QStringLiteral("order_progress")) {
                raw.prepare(QStringLiteral("UPDATE charging_orders SET updated_at='2026-09-06 04:05:06' WHERE id=?"));
                raw.addBindValue(orderId); QVERIFY(raw.exec());
                QVERIFY(orders.updateProgress(orderId, 10, 100, 10, &error));
                sql = QStringLiteral("SELECT updated_at FROM charging_orders WHERE id=?");
            } else if (operation == QStringLiteral("order_settle")) {
                QVERIFY(orders.stopAndSettle(orderId, 10, 100, 10, QStringLiteral("completed"),
                                            QStringLiteral("test_stop"), nullptr, &error));
                sql = QStringLiteral("SELECT o.stopped_at,o.updated_at,w.created_at,u.updated_at,p.updated_at "
                    "FROM charging_orders o JOIN wallet_records w ON w.order_id=o.id "
                    "JOIN users u ON u.id=o.user_id JOIN charging_piles p ON p.id=o.pile_id WHERE o.id=?");
            } else {
                sql = QStringLiteral("SELECT o.started_at,o.created_at,o.updated_at,p.updated_at "
                    "FROM charging_orders o JOIN charging_piles p ON p.id=o.pile_id WHERE o.id=?");
                if (reservationId) {
                    raw.prepare(QStringLiteral("SELECT used_at FROM reservations WHERE id=?"));
                    raw.addBindValue(reservationId); QVERIFY(raw.exec()); QVERIFY(raw.next());
                    const QString used = raw.value(0).toString();
                    QCOMPARE(used, QDateTime::fromString(used, Qt::ISODate).toUTC().toString(Qt::ISODate));
                    QCOMPARE(used.size(), 20);
                    QVERIFY(QDateTime::fromString(used, Qt::ISODate) >= earliest);
                    QVERIFY(QDateTime::fromString(used, Qt::ISODate) <= QDateTime::currentDateTimeUtc());
                }
            }
            targetId = orderId;
        }
    }
    QVERIFY(!sql.isEmpty());
    raw.prepare(sql); raw.addBindValue(targetId);
    QVERIFY2(raw.exec(), qPrintable(raw.lastError().text()));
    QVERIFY(raw.next());
    for (int column = 0; column < raw.record().count(); ++column) {
        const QString timestamp = raw.value(column).toString();
        const QDateTime parsed = QDateTime::fromString(timestamp, Qt::ISODate);
        QVERIFY2(parsed.isValid(), qPrintable(timestamp));
        QCOMPARE(timestamp, parsed.toUTC().toString(Qt::ISODate));
        QCOMPARE(timestamp.size(), 20);
        if (!(operation == QStringLiteral("reservation_create") && column == 1)) {
            QVERIFY(parsed >= earliest);
            QVERIFY(parsed <= QDateTime::currentDateTimeUtc());
        }
    }
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
