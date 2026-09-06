#include "service_test.h"
#include "repositories/userrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/pilerepository.h"
#include "services/userservice.h"
#include "services/stationservice.h"
#include "services/pileservice.h"
#include "services/serviceresult.h"
#include "services/reservationservice.h"
#include "services/orderservice.h"
#include "services/billingservice.h"
#include "repositories/reservationrepository.h"
#include "repositories/orderrepository.h"
#include "repositories/walletrepository.h"
#include "repositories/alarmrepository.h"
#include "services/chargingservice.h"
#include "services/adminservice.h"
#include "services/statisticsservice.h"
#include <limits>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSemaphore>
#include <QtTest>
#include <future>

using Charging::ErrorCode;

namespace {
QString imageBase64(const char *format = "PNG")
{
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format);
    return QString::fromLatin1(bytes.toBase64());
}
}

void ServiceTest::initTestCase()
{
    QVERIFY(m_directory.isValid());
    QString error;
    QVERIFY2(m_database.open(m_directory.filePath("services.db"), &error), qPrintable(error));
}

qint64 ServiceTest::createUser(const QString &phone)
{
    UserRecord user;
    bool created = false;
    QString error;
    if (!UserRepository(&m_database).findOrCreate(phone, &user, &created, &error)) {
        QTest::qFail(qPrintable(error), __FILE__, __LINE__);
        return 0;
    }
    return user.id;
}

void ServiceTest::profileValidationAndPartialUpdate()
{
    UserService service(&m_database);
    const auto id = createUser("13800138101");
    for (const QJsonObject &payload : QList<QJsonObject>{
             {}, {{"nickname", 3}}, {{"nickname", ""}}, {{"nickname", "a"}},
             {{"nickname", QString(21, 'a')}}, {{"nickname", "bad!"}},
             {{"nickname", "ab\ncd"}}, {{"nickname", "车主😀"}}, {{"avatarBase64", true}}})
        QCOMPARE(service.updateProfile(id, payload).error, ErrorCode::ValidationFailed);
    auto result = service.updateProfile(id, {{"nickname", "  易子恒 abc_9-  "}});
    QVERIFY(result.succeeded());
    QCOMPARE(result.payload.value("nickname").toString(), QString("易子恒 abc_9-"));
    QCOMPARE(result.payload.value("phone").toString(), QString("13800138101"));
    QVERIFY(result.payload.contains("avatar"));
    QCOMPARE(result.payload.value("balanceCents").toInt(), 0);
    QVERIFY(service.updateProfile(id, {{"nickname", QString(20, 'a')}}).succeeded());
    QCOMPARE(service.updateProfile(999999, {{"nickname", "车主"}}).error, ErrorCode::NotFound);
}

void ServiceTest::avatarValidationAndPersistence()
{
    UserService service(&m_database);
    const auto id = createUser("13800138102");
    const QString png = imageBase64();
    QByteArray boundaryImage = QByteArray::fromBase64(png.toLatin1());
    boundaryImage.append(QByteArray(2 * 1024 * 1024 - boundaryImage.size(), '\0'));
    QVERIFY(!QImage::fromData(boundaryImage).isNull());
    QByteArray oversizedImage = boundaryImage + '\0';
    for (const auto &value : QStringList{"not-base64", "AAAA", "data:image/gif;base64," + png,
             "data:image/jpeg;base64," + png,
             QString::fromLatin1(oversizedImage.toBase64())})
        QCOMPARE(service.updateProfile(id, {{"avatarBase64", value}}).error,
                 ErrorCode::ValidationFailed);
    QVERIFY(service.updateProfile(id, {{"avatarBase64", QString::fromLatin1(boundaryImage.toBase64())}}).succeeded());
    auto result = service.updateProfile(id, {{"avatarBase64", "data:image/png;base64," + png}});
    QVERIFY(result.succeeded());
    QString error;
    UserRecord user;
    QVERIFY(UserRepository(&m_database).findById(id, &user, &error));
    const QString firstPath = m_directory.filePath(user.avatarPath);
    QFile file(firstPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray::fromBase64(png.toLatin1()));
    QVERIFY(!QImage(firstPath).isNull());
    const QString nickname = user.nickname;
    QVERIFY(service.updateProfile(id, {{"avatarBase64", imageBase64("JPEG")}}).succeeded());
    QVERIFY(UserRepository(&m_database).findById(id, &user, &error));
    QCOMPARE(user.nickname, nickname);
    QVERIFY(!QImage(m_directory.filePath(user.avatarPath)).isNull());
    QVERIFY(service.updateProfile(id, {{"nickname", "新昵称"}}).succeeded());
    UserRecord updated;
    QVERIFY(UserRepository(&m_database).findById(id, &updated, &error));
    QCOMPARE(updated.avatarPath, user.avatarPath);
}

void ServiceTest::concurrentIndependentProfileUpdates()
{
    const auto id = createUser("13800138109");
    const QString avatar = imageBase64();
    for (int iteration = 0; iteration < 32; ++iteration) {
        QSemaphore ready;
        QSemaphore start;
        const QString nickname = QString("并发车主%1").arg(iteration);
        auto update = [&](QJsonObject payload) {
            // Open each real worker connection before releasing both requests.
            m_database.database();
            ready.release();
            start.acquire();
            return UserService(&m_database).updateProfile(id, payload);
        };
        auto nicknameRequest = std::async(std::launch::async, update, QJsonObject{{"nickname", nickname}});
        auto avatarRequest = std::async(std::launch::async, update, QJsonObject{{"avatarBase64", avatar}});
        ready.acquire(2);
        start.release(2);
        const auto nicknameResult = nicknameRequest.get();
        const auto avatarResult = avatarRequest.get();
        QVERIFY(nicknameResult.succeeded());
        QVERIFY(avatarResult.succeeded());
        UserRecord user;
        QString error;
        QVERIFY(UserRepository(&m_database).findById(id, &user, &error));
        QCOMPARE(user.nickname, nickname);
        QCOMPARE(user.avatarPath, avatarResult.payload.value("avatar").toString());
        QVERIFY(QFile::exists(m_directory.filePath(user.avatarPath)));
    }
}

void ServiceTest::profileResponseReflectsCommittedRow()
{
    const auto id = createUser("13800138110");
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER enrich_profile AFTER UPDATE OF nickname ON users BEGIN UPDATE users SET balance_cents=123 WHERE id=NEW.id; END"));
    const auto result = UserService(&m_database).updateProfile(id, {{"nickname", "更新后资料"}});
    QVERIFY(query.exec("DROP TRIGGER enrich_profile"));
    QVERIFY(result.succeeded());
    QCOMPARE(result.payload.value("balanceCents").toInt(), 123);
}

void ServiceTest::avatarDatabaseFailureKeepsOldFile()
{
    UserService service(&m_database);
    const auto id = createUser("13800138103");
    auto first = service.updateProfile(id, {{"avatarBase64", imageBase64()}});
    QVERIFY(first.succeeded());
    const QDir avatars(m_directory.filePath("avatars"));
    const auto before = avatars.entryList(QDir::Files);
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER reject_profile BEFORE UPDATE ON users BEGIN SELECT RAISE(ABORT,'test update failure'); END"));
    const auto failed = service.updateProfile(id, {{"avatarBase64", imageBase64("JPEG")}});
    QVERIFY(query.exec("DROP TRIGGER reject_profile"));
    QCOMPARE(failed.error, ErrorCode::DatabaseError);
    QCOMPARE(avatars.entryList(QDir::Files), before);
    QVERIFY(QFile::exists(m_directory.filePath(first.payload.value("avatar").toString())));
}

void ServiceTest::rechargeBoundsAndIdempotency()
{
    const auto id = createUser("13800138104");
    UserService service(&m_database);
    for (const QJsonValue &amount : QList<QJsonValue>{99, 1000001, 100.5, "100", true, QJsonValue()})
        QCOMPARE(service.recharge(id, {{"amountCents", amount}, {"transactionId", "bounds"}}).error,
                 ErrorCode::ValidationFailed);
    QCOMPARE(service.recharge(id, {{"amountCents", 100}}).error, ErrorCode::ValidationFailed);
    QCOMPARE(service.recharge(id, {{"amountCents", 100}, {"transactionId", "  "}}).error,
             ErrorCode::ValidationFailed);
    const QJsonObject request{{"amountCents", 100}, {"transactionId", "recharge-first"}};
    const auto first = service.recharge(id, request);
    QVERIFY(first.succeeded());
    QCOMPARE(first.payload.value("balanceCents").toInt(), 100);
    QVERIFY(first.payload.value("recordId").toDouble() > 0);
    QVERIFY(first.payload.value("createdAt").toString().endsWith('Z'));
    QVERIFY(service.recharge(id, {{"amountCents", 1000000}, {"transactionId", "recharge-max"}}).succeeded());
    QCOMPARE(service.recharge(id, request).payload, first.payload);
    QCOMPARE(service.recharge(id, {{"amountCents", 200}, {"transactionId", "recharge-first"}}).error,
             ErrorCode::Conflict);
    const auto otherId = createUser("13800138105");
    QCOMPARE(service.recharge(otherId, request).error, ErrorCode::Conflict);
    UserRecord user;
    QString error;
    QVERIFY(UserRepository(&m_database).findById(id, &user, &error));
    QCOMPARE(user.balanceCents, qint64(1000100));
}

void ServiceTest::walletPaginationAndOrdering()
{
    const auto id = createUser("13800138106");
    UserService service(&m_database);
    for (int i = 1; i <= 3; ++i)
        QVERIFY(service.recharge(id, {{"amountCents", i * 100},
                                      {"transactionId", QString("ledger-%1").arg(i)}}).succeeded());
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("UPDATE wallet_records SET created_at=CASE record_no WHEN 'ledger-1' THEN '2026-09-02 08:00:00' WHEN 'ledger-2' THEN '2026-09-01T12:00:00Z' WHEN 'ledger-3' THEN '2026-09-01T12:00:00Z' ELSE created_at END"));
    auto page = service.walletLedger(id, {{"page", 1}, {"pageSize", 2}});
    QVERIFY(page.succeeded());
    QCOMPARE(page.payload.value("total").toInt(), 3);
    auto items = page.payload.value("items").toArray();
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].toObject().value("amountCents").toInt(), 100);
    QCOMPARE(items[1].toObject().value("amountCents").toInt(), 300);
    QCOMPARE(items[0].toObject().value("createdAt").toString(), QString("2026-09-02T08:00:00Z"));
    page = service.walletLedger(id, {{"page", 2}, {"pageSize", 2}});
    QCOMPARE(page.payload.value("items").toArray()[0].toObject().value("amountCents").toInt(), 200);
    QCOMPARE(service.walletLedger(id, {{"page", 3}, {"pageSize", 2}}).payload.value("items").toArray().size(), 0);
    for (const QJsonObject &bad : QList<QJsonObject>{{{"page", 0}}, {{"page", 1.5}},
             {{"pageSize", 101}}, {{"pageSize", "2"}}, {{"page", 2147483647}, {"pageSize", 100}}})
        QCOMPARE(service.walletLedger(id, bad).error, ErrorCode::ValidationFailed);
}

void ServiceTest::nearbyTextSearchAndCoordinates()
{
    const auto id = createUser("13800138107");
    StationRepository repository(&m_database);
    qint64 farId, nearId, outsideId;
    QString error;
    StationRecord station;
    station.name = "赤道测试远站"; station.address = "测试区北路";
    station.latitude = 0; station.longitude = 0.02; station.priceCentsPerKwh = 120;
    QVERIFY(repository.insert(station, &farId, &error));
    station.name = "赤道测试近站"; station.longitude = 0.01;
    QVERIFY(repository.insert(station, &nearId, &error));
    station.name = "赤道测试外站"; station.longitude = 0.2;
    QVERIFY(repository.insert(station, &outsideId, &error));
    StationService service(&m_database);
    auto result = service.nearby(id, {{"region", "测试区"}, {"address", "近站"}});
    QVERIFY(result.succeeded());
    auto items = result.payload.value("items").toArray();
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].toObject().value("stationId").toDouble(), double(nearId));
    QVERIFY(items[0].toObject().value("distanceKm").isNull());
    result = service.nearby(id, {{"latitude", 0}, {"longitude", 0}, {"radiusKm", 3}, {"sort", "distance"}});
    QVERIFY(result.succeeded());
    items = result.payload.value("items").toArray();
    QCOMPARE(items.size(), 2);
    const auto first = items[0].toObject();
    QCOMPARE(first.value("stationId").toDouble(), double(nearId));
    QCOMPARE(first.value("longitude").toDouble(), 0.01);
    QCOMPARE(first.value("distanceKm").toDouble(), 1.11);
    QCOMPARE(items[1].toObject().value("distanceKm").toDouble(), 2.22);
    QVERIFY(first.contains("availablePiles"));
    QVERIFY(first.contains("totalPiles"));
    QVERIFY(service.nearby(id, {{"address", "不存在的地址"}}).payload.value("items").toArray().isEmpty());
    for (const QJsonObject &bad : QList<QJsonObject>{{{"latitude", 0}}, {{"latitude", 91}, {"longitude", 0}},
             {{"latitude", "0"}, {"longitude", 0}}, {{"radiusKm", 0}}, {{"radiusKm", 101}},
             {{"address", true}}, {{"sort", "wrong"}}})
        QCOMPARE(service.nearby(id, bad).error, ErrorCode::ValidationFailed);
}

void ServiceTest::pileStatusAndFavoriteIdempotency()
{
    const auto id = createUser("13800138108");
    PileService piles(&m_database);
    auto result = piles.listForStation({{"stationId", 1}});
    QVERIFY(result.succeeded());
    auto items = result.payload.value("items").toArray();
    QVERIFY(!items.isEmpty());
    QCOMPARE(items[0].toObject().value("status").toString(), QString("available"));
    QVERIFY(items[0].toObject().contains("type"));
    QVERIFY(items[0].toObject().contains("priceCentsPerKwh"));
    QVERIFY(items[0].toObject().value("updatedAt").toString().endsWith('Z'));
    QCOMPARE(piles.listForStation({{"stationId", "1"}}).error, ErrorCode::ValidationFailed);
    QCOMPARE(piles.listForStation({{"stationId", 1.5}}).error, ErrorCode::ValidationFailed);
    QCOMPARE(piles.listForStation({{"stationId", 999999}}).error, ErrorCode::NotFound);
    StationService stations(&m_database);
    for (bool favorite : {true, true, false, false}) {
        const auto response = stations.toggleFavorite(id, {{"stationId", 1}, {"favorited", favorite}});
        QVERIFY(response.succeeded());
        QCOMPARE(response.payload.value("favorited").toBool(), favorite);
        QVERIFY(response.payload.value("updatedAt").toString().endsWith('Z'));
        const auto nearby = stations.nearby(id, {}).payload.value("items").toArray();
        QCOMPARE(nearby.first().toObject().value("favorited").toBool(), favorite);
    }
    QCOMPARE(stations.toggleFavorite(id, {{"stationId", 1}, {"favorited", "true"}}).error, ErrorCode::ValidationFailed);
    QCOMPARE(stations.toggleFavorite(id, {{"stationId", 999999}, {"favorited", true}}).error, ErrorCode::NotFound);
}

void ServiceTest::databaseFailures()
{
    DatabaseManager unavailable;
    UserService users(&unavailable);
    StationService stations(&unavailable);
    PileService piles(&unavailable);
    QCOMPARE(users.updateProfile(1, {{"nickname", "车主"}}).error, ErrorCode::DatabaseError);
    QCOMPARE(users.recharge(1, {{"amountCents", 100}, {"transactionId", "unavailable"}}).error, ErrorCode::DatabaseError);
    QCOMPARE(users.walletLedger(1, {}).error, ErrorCode::DatabaseError);
    QCOMPARE(stations.nearby(1, {}).error, ErrorCode::DatabaseError);
    QCOMPARE(stations.toggleFavorite(1, {{"stationId", 1}, {"favorited", true}}).error, ErrorCode::DatabaseError);
    QCOMPARE(piles.listForStation({{"stationId", 1}}).error, ErrorCode::DatabaseError);
}

void ServiceTest::reservationSelectionAndDuration()
{
    const auto id = createUser("13800138201");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 1000}, {"transactionId", "reserve-selection"}}).succeeded());
    ReservationService service(&m_database);
    auto result = service.create(id, {{"stationId", 1}});
    QVERIFY(result.succeeded());
    QCOMPARE(result.payload.value("stationId").toInt(), 1);
    const auto pileId = result.payload.value("pileId").toInt();
    QVERIFY(pileId == 1 || pileId == 2);
    QCOMPARE(result.payload.value("status").toString(), QString("active"));
    QVERIFY(qAbs(QDateTime::fromString(result.payload.value("reservedAt").toString(), Qt::ISODate)
                 .secsTo(QDateTime::fromString(result.payload.value("expiresAt").toString(), Qt::ISODate)) - 900) <= 1);
    PileRecord pile;
    QString error;
    QVERIFY(PileRepository(&m_database).findById(pileId, &pile, &error));
    QCOMPARE(pile.status, QString("reserved"));
    QVERIFY(service.cancel(id, {{"reservationId", result.payload.value("reservationId")}}).succeeded());
    for (int minutes : {5, 30}) {
        result = service.create(id, {{"stationId", 1}, {"pileId", 2}, {"durationMinutes", minutes}});
        QVERIFY(result.succeeded());
        QCOMPARE(result.payload.value("pileId").toInt(), 2);
        const auto expiry = QDateTime::fromString(result.payload.value("expiresAt").toString(), Qt::ISODate);
        QVERIFY(qAbs(QDateTime::currentDateTimeUtc().secsTo(expiry) - minutes * 60) <= 1);
        QVERIFY(service.cancel(id, {{"reservationId", result.payload.value("reservationId")}}).succeeded());
    }
}

void ServiceTest::reservationValidationAndConflicts()
{
    const auto id = createUser("13800138202");
    ReservationService service(&m_database);
    for (const QJsonObject &bad : QList<QJsonObject>{{}, {{"stationId", "1"}}, {{"stationId", 1.5}},
             {{"stationId", 1}, {"pileId", 0}}, {{"stationId", 1}, {"pileId", "1"}},
             {{"stationId", 1}, {"durationMinutes", 4}}, {{"stationId", 1}, {"durationMinutes", 31}},
             {{"stationId", 1}, {"durationMinutes", 5.5}}, {{"stationId", 1}, {"durationMinutes", "15"}}})
        QCOMPARE(service.create(id, bad).error, ErrorCode::ValidationFailed);
    auto result = service.create(id, {{"stationId", 1}});
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("insufficient_balance"));
    QString error;
    QVERIFY(UserRepository(&m_database).setStatus(id, "frozen", &error));
    result = service.create(id, {{"stationId", 1}});
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("user_frozen"));
    QVERIFY(UserRepository(&m_database).setStatus(id, "normal", &error));
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 1000}, {"transactionId", "reserve-conflicts"}}).succeeded());
    QCOMPARE(service.create(999999, {{"stationId", 1}}).error, ErrorCode::NotFound);
    QCOMPARE(service.create(id, {{"stationId", 999999}}).error, ErrorCode::NotFound);
    QCOMPARE(service.create(id, {{"stationId", 1}, {"pileId", 999999}}).error, ErrorCode::NotFound);
    QCOMPARE(service.create(id, {{"stationId", 1}, {"pileId", 3}}).error, ErrorCode::ValidationFailed);
    QVERIFY(PileRepository(&m_database).updateStatus(1, "idle", "offline", &error));
    result = service.create(id, {{"stationId", 1}, {"pileId", 1}});
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("pile_unavailable"));
    result = service.create(id, {{"stationId", 1}});
    QVERIFY(result.succeeded());
    QCOMPARE(result.payload.value("pileId").toInt(), 2);
    const auto reservationId = result.payload.value("reservationId");
    result = service.create(id, {{"stationId", 2}});
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("reservation_conflict"));
    const auto other = createUser("13800138203");
    QVERIFY(UserService(&m_database).recharge(other, {{"amountCents", 100}, {"transactionId", "reserve-other"}}).succeeded());
    result = service.create(other, {{"stationId", 1}});
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("pile_unavailable"));
    QVERIFY(service.cancel(id, {{"reservationId", reservationId}}).succeeded());
    QVERIFY(PileRepository(&m_database).updateStatus(1, "offline", "idle", &error));
}

void ServiceTest::reservationCancelAndExpiry()
{
    const auto id = createUser("13800138204");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 1000}, {"transactionId", "reserve-cancel"}}).succeeded());
    ReservationService service(&m_database);
    auto created = service.create(id, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(created.succeeded());
    const QJsonObject request{{"reservationId", created.payload.value("reservationId")}};
    QCOMPARE(service.cancel(id, {}).error, ErrorCode::ValidationFailed);
    QCOMPARE(service.cancel(id, {{"reservationId", "1"}}).error, ErrorCode::ValidationFailed);
    QCOMPARE(service.cancel(id, {{"reservationId", 999999}}).error, ErrorCode::NotFound);
    QCOMPARE(service.cancel(createUser("13800138205"), request).error, ErrorCode::Forbidden);
    QVERIFY(service.cancel(id, request).succeeded());
    QCOMPARE(service.cancel(id, request).payload.value("status").toString(), QString("cancelled"));
    PileRecord pile;
    QString error;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
    created = service.create(id, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(created.succeeded());
    const auto expiry = QDateTime::fromString(created.payload.value("expiresAt").toString(), Qt::ISODate);
    QCOMPARE(service.expireDue(QDateTime()).error, ErrorCode::ValidationFailed);
    QCOMPARE(service.expireDue(expiry.addSecs(-1)).payload.value("expiredCount").toInt(), 0);
    QCOMPARE(service.expireDue(expiry).payload.value("expiredCount").toInt(), 1);
    QCOMPARE(service.expireDue(expiry).payload.value("expiredCount").toInt(), 0);
    const auto expired = service.cancel(id, {{"reservationId", created.payload.value("reservationId")}});
    QCOMPARE(expired.error, ErrorCode::Conflict);
    QCOMPARE(expired.reason, QString("reservation_expired"));
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
}

void ServiceTest::reservationWriteFailureRollsBack()
{
    const auto id = createUser("13800138206");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "reserve-failure"}}).succeeded());
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER reject_reservation BEFORE INSERT ON reservations BEGIN SELECT RAISE(ABORT,'test reservation failure'); END"));
    const auto result = ReservationService(&m_database).create(id, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(query.exec("DROP TRIGGER reject_reservation"));
    QCOMPARE(result.error, ErrorCode::DatabaseError);
    PileRecord pile;
    QString error;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
}

void ServiceTest::activeOrderAndSettlement()
{
    const auto id = createUser("13800138207");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 1000}, {"transactionId", "order-settle"}}).succeeded());
    OrderService service(&m_database);
    QCOMPARE(service.active(id).payload, QJsonObject({{"active", false}}));
    QCOMPARE(service.active(999999).error, ErrorCode::NotFound);
    const auto reservation = ReservationService(&m_database).create(id, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(reservation.succeeded());
    QString error;
    qint64 orderId = 0;
    OrderRepository orders(&m_database);
    QVERIFY(orders.createChargingOrder("service-order-1", id, 1, reservation.payload.value("reservationId").toInt(), &orderId, &error));
    QVERIFY(orders.updateProgress(orderId, 120, 1234, 148, &error));
    auto result = service.active(id);
    QVERIFY(result.succeeded());
    QCOMPARE(result.payload.value("active").toBool(), true);
    QCOMPARE(result.payload.value("orderId").toDouble(), double(orderId));
    QCOMPARE(result.payload.value("status").toString(), QString("charging"));
    QCOMPARE(result.payload.value("payableCents").toInt(), 148);
    QCOMPARE(result.payload.value("energyKwh").toDouble(), 1.234);
    QCOMPARE(result.payload.value("durationSec").toInt(), 120);
    QVERIFY(result.payload.value("startedAt").toString().endsWith('Z'));
    auto conflict = ReservationService(&m_database).create(id, {{"stationId", 2}});
    QCOMPARE(conflict.error, ErrorCode::Conflict);
    QCOMPARE(conflict.reason, QString("order_conflict"));
    BillingService billing(&m_database);
    result = billing.settle(orderId, 121, 1240, "completed", "user_stop");
    QVERIFY(result.succeeded());
    QCOMPARE(result.payload.value("status").toString(), QString("completed"));
    QCOMPARE(result.payload.value("payableCents").toInt(), 148);
    QCOMPARE(result.payload.value("balanceCents").toInt(), 852);
    QCOMPARE(result.payload.value("stopReason").toString(), QString("user_stop"));
    QVERIFY(result.payload.value("stoppedAt").toString().endsWith('Z'));
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "after-settle"}}).succeeded());
    QCOMPARE(billing.settle(orderId, 999, 9999, "fault_stopped", "fault").payload, result.payload);
    QCOMPARE(service.active(id).payload, QJsonObject({{"active", false}}));
    PileRecord pile;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
    QCOMPARE(pile.totalChargeCount, 1);
    UserRecord user;
    QVERIFY(UserRepository(&m_database).findById(id, &user, &error));
    QCOMPARE(user.balanceCents, qint64(952));
}

void ServiceTest::billingRoundingAndValidation()
{
    QCOMPARE(BillingService::feeCents(0, 120), qint64(0));
    QCOMPARE(BillingService::feeCents(8, 120), qint64(0));
    QCOMPARE(BillingService::feeCents(9, 120), qint64(1));
    QCOMPARE(BillingService::feeCents(1234, 120), qint64(148));
    QCOMPARE(BillingService::feeCents(999, 999), qint64(998));
    // Multiplication overflows qint64 even though the final amount fits.
    QCOMPARE(BillingService::feeCents(1000000000000000000LL, 120), qint64(120000000000000000LL));
    QCOMPARE(BillingService::feeCents(-1, 120), qint64(-1));
    QCOMPARE(BillingService::feeCents(1000, -1), qint64(-1));
    QCOMPARE(BillingService::feeCents(std::numeric_limits<qint64>::max(), 1001), qint64(-1));
    BillingService service(&m_database);
    QCOMPARE(service.settle(999999, 1, 1, "completed", "user_stop").error, ErrorCode::NotFound);
    QCOMPARE(service.settle(1, -1, 1, "completed", "user_stop").error, ErrorCode::ValidationFailed);
    QCOMPARE(service.settle(1, 1, -1, "completed", "user_stop").error, ErrorCode::ValidationFailed);
    QCOMPARE(service.settle(1, 1, 1, "charging", "user_stop").error, ErrorCode::ValidationFailed);
    DatabaseManager unavailable;
    QCOMPARE(ReservationService(&unavailable).create(1, {{"stationId", 1}}).error, ErrorCode::DatabaseError);
    QCOMPARE(ReservationService(&unavailable).cancel(1, {{"reservationId", 1}}).error, ErrorCode::DatabaseError);
    QCOMPARE(ReservationService(&unavailable).expireDue(QDateTime::currentDateTimeUtc()).error, ErrorCode::DatabaseError);
    QCOMPARE(OrderService(&unavailable).active(1).error, ErrorCode::DatabaseError);
    QCOMPARE(BillingService(&unavailable).settle(1, 1, 1, "completed", "user_stop").error, ErrorCode::DatabaseError);
}

void ServiceTest::settlementFailureRollsBack()
{
    const auto id = createUser("13800138208");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "settle-failure"}}).succeeded());
    QString error;
    qint64 orderId;
    QVERIFY(OrderRepository(&m_database).createChargingOrder("service-order-failure", id, 1, 0, &orderId, &error));
    BillingService billing(&m_database);
    auto result = billing.settle(orderId, 60, 1000, "completed", "user_stop");
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("insufficient_balance"));
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER reject_payment BEFORE INSERT ON wallet_records WHEN NEW.record_type='charge_payment' BEGIN SELECT RAISE(ABORT,'test payment failure'); END"));
    result = billing.settle(orderId, 60, 500, "completed", "user_stop");
    QVERIFY(query.exec("DROP TRIGGER reject_payment"));
    QCOMPARE(result.error, ErrorCode::DatabaseError);
    UserRecord user;
    QVERIFY(UserRepository(&m_database).findById(id, &user, &error));
    QCOMPARE(user.balanceCents, qint64(100));
    QCOMPARE(OrderService(&m_database).active(id).payload.value("status").toString(), QString("charging"));
    PileRecord pile;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("charging"));
    QVERIFY(billing.settle(orderId, 60, 500, "fault_stopped", "device_fault").succeeded());
}

void ServiceTest::reservationTransactionRechecksEligibility()
{
    const auto id = createUser("13800138209");
    ReservationRepository reservations(&m_database);
    QString error;
    qint64 reservationId = 0;
    const auto expires = QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODate);
    // These repository calls represent a service precheck that became stale before its write.
    QVERIFY(!reservations.create(id, 2, expires, &reservationId, &error));
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "reserve-recheck"}}).succeeded());
    qint64 orderId = 0;
    QVERIFY(OrderRepository(&m_database).createChargingOrder("service-order-recheck", id, 1, 0, &orderId, &error));
    QVERIFY(!reservations.create(id, 2, expires, &reservationId, &error));
    PileRecord pile;
    QVERIFY(PileRepository(&m_database).findById(2, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
    QVERIFY(BillingService(&m_database).settle(orderId, 0, 0, "completed", "user_stop").succeeded());
}

void ServiceTest::expiryPreservesUnavailablePiles()
{
    const auto id = createUser("13800138210");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "reserve-fault"}}).succeeded());
    ReservationService service(&m_database);
    const auto created = service.create(id, {{"stationId", 1}, {"pileId", 2}});
    QVERIFY(created.succeeded());
    QString error;
    QVERIFY(PileRepository(&m_database).updateStatus(2, "reserved", "fault", &error));
    const auto expires = QDateTime::fromString(created.payload.value("expiresAt").toString(), Qt::ISODate);
    QVERIFY(service.expireDue(expires).succeeded());
    PileRecord pile;
    QVERIFY(PileRepository(&m_database).findById(2, &pile, &error));
    QCOMPARE(pile.status, QString("fault"));
    QVERIFY(PileRepository(&m_database).updateStatus(2, "fault", "idle", &error));
}

void ServiceTest::stationChangeAfterPrecheckRejectsReservation()
{
    const auto id = createUser("13800138211");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "reserve-station-race"}}).succeeded());
    QSqlQuery query(m_database.database());
    // The service has already read the online station before it updates the pile.
    // A real SQLite trigger deterministically changes that state at the write boundary.
    QVERIFY(query.exec("CREATE TRIGGER station_offline_at_reserve AFTER UPDATE OF status ON charging_piles "
                       "WHEN NEW.id=2 AND NEW.status='reserved' BEGIN UPDATE stations SET status='offline' WHERE id=NEW.station_id; END"));
    const auto result = ReservationService(&m_database).create(id, {{"stationId", 1}, {"pileId", 2}});
    QVERIFY(query.exec("DROP TRIGGER station_offline_at_reserve"));
    QCOMPARE(result.error, ErrorCode::Conflict);
    QCOMPARE(result.reason, QString("pile_unavailable"));
    ReservationRecord reservation;
    PileRecord pile;
    QString error;
    QVERIFY(ReservationRepository(&m_database).findActiveByUser(id, &reservation, &error));
    QCOMPARE(reservation.id, qint64(0));
    QVERIFY(PileRepository(&m_database).findById(2, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
}

void ServiceTest::simultaneousReservations()
{
    const auto firstId = createUser("13800138212");
    const auto secondId = createUser("13800138213");
    QVERIFY(UserService(&m_database).recharge(firstId, {{"amountCents", 100}, {"transactionId", "reserve-parallel-1"}}).succeeded());
    QVERIFY(UserService(&m_database).recharge(secondId, {{"amountCents", 100}, {"transactionId", "reserve-parallel-2"}}).succeeded());
    // Same pile/different users, different piles/same user, then automatic selection.
    for (int scenario = 0; scenario < 3; ++scenario) {
        for (int iteration = 0; iteration < 32; ++iteration) {
            QSemaphore ready, start;
            auto request = [&](qint64 userId, QJsonObject payload) {
                m_database.database();
                ready.release();
                start.acquire();
                return ReservationService(&m_database).create(userId, payload);
            };
            const auto otherId = scenario == 1 ? firstId : secondId;
            const QJsonObject firstPayload = scenario == 2 ? QJsonObject{{"stationId", 1}}
                : QJsonObject{{"stationId", 1}, {"pileId", 1}};
            const QJsonObject secondPayload = scenario == 2 ? QJsonObject{{"stationId", 1}}
                : QJsonObject{{"stationId", 1}, {"pileId", scenario == 1 ? 2 : 1}};
            auto firstRequest = std::async(std::launch::async, request, firstId, firstPayload);
            auto secondRequest = std::async(std::launch::async, request, otherId, secondPayload);
            ready.acquire(2);
            start.release(2);
            const auto first = firstRequest.get();
            const auto second = secondRequest.get();
            if (first.succeeded())
                QVERIFY(ReservationService(&m_database).cancel(firstId, {{"reservationId", first.payload.value("reservationId")}}).succeeded());
            if (second.succeeded())
                QVERIFY(ReservationService(&m_database).cancel(otherId, {{"reservationId", second.payload.value("reservationId")}}).succeeded());
            if (scenario == 2) {
                QVERIFY2(first.succeeded(), qPrintable(QString("first automatic: %1 %2").arg(int(first.error)).arg(first.reason)));
                QVERIFY2(second.succeeded(), qPrintable(QString("second automatic: %1 %2").arg(int(second.error)).arg(second.reason)));
                QVERIFY(first.payload.value("pileId") != second.payload.value("pileId"));
            } else {
                QCOMPARE(int(first.succeeded()) + int(second.succeeded()), 1);
                const auto rejected = first.succeeded() ? second : first;
                QCOMPARE(rejected.error, ErrorCode::Conflict);
                QCOMPARE(rejected.reason, scenario == 0 ? QString("pile_unavailable") : QString("reservation_conflict"));
            }
        }
    }
}

void ServiceTest::directOrderCannotBypassActiveReservation()
{
    const auto id = createUser("13800138214");
    QVERIFY(UserService(&m_database).recharge(id, {{"amountCents", 100}, {"transactionId", "order-reservation-guard"}}).succeeded());
    const auto result = ReservationService(&m_database).create(id, {{"stationId", 1}, {"pileId", 2}});
    QVERIFY(result.succeeded());
    const auto reservationId = qint64(result.payload.value("reservationId").toDouble());
    OrderRepository orders(&m_database);
    qint64 orderId = 0;
    QString error;
    QVERIFY(!orders.createChargingOrder("direct-while-reserved", id, 1, 0, &orderId, &error));
    QCOMPARE(error, QString("reservation_conflict"));
    OrderRecord active;
    QVERIFY(orders.findActiveByUser(id, &active, &error));
    QCOMPARE(active.id, qint64(0));
    PileRecord pile;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("idle"));
    ReservationRecord reservation;
    QVERIFY(ReservationRepository(&m_database).findById(reservationId, &reservation, &error));
    QCOMPARE(reservation.status, QString("active"));
    QVERIFY(orders.createChargingOrder("start-matching-reservation", id, 2, reservationId, &orderId, &error));
    QVERIFY(BillingService(&m_database).settle(orderId, 0, 0, "completed", "user_stop").succeeded());
}

void ServiceTest::reservationContentionWaitsBeforeReading()
{
    const auto firstId = createUser("13800138215");
    const auto secondId = createUser("13800138216");
    QVERIFY(UserService(&m_database).recharge(firstId, {{"amountCents", 100}, {"transactionId", "reserve-lock-1"}}).succeeded());
    QVERIFY(UserService(&m_database).recharge(secondId, {{"amountCents", 100}, {"transactionId", "reserve-lock-2"}}).succeeded());
    QSemaphore ready, start;
    auto request = [&](qint64 userId) {
        m_database.database();
        ready.release();
        start.acquire();
        QString error;
        qint64 id = 0;
        const bool succeeded = ReservationRepository(&m_database).create(userId, 1,
            QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODate), &id, &error);
        m_database.releaseCurrentThreadConnection();
        return std::make_pair(succeeded, error);
    };
    // Preopen worker connections so initialization is outside the contended section.
    auto firstRequest = std::async(std::launch::async, request, firstId);
    auto secondRequest = std::async(std::launch::async, request, secondId);
    ready.acquire(2);
    QSqlQuery blocker(m_database.database());
    const bool acquired = blocker.exec("BEGIN IMMEDIATE");
    start.release(2);
    // Deferred transactions can read here but fail immediately when upgrading to writes.
    // An immediate transaction must wait, without starting a stale read snapshot.
    firstRequest.wait_for(std::chrono::milliseconds(100));
    secondRequest.wait_for(std::chrono::milliseconds(100));
    const bool released = blocker.exec("COMMIT");
    const auto first = firstRequest.get();
    const auto second = secondRequest.get();
    QVERIFY(acquired);
    QVERIFY(released);
    QCOMPARE(int(first.first) + int(second.first), 1);
    const auto rejected = first.first ? second : first;
    QCOMPARE(rejected.second, QString("pile_unavailable"));
    const auto winner = first.first ? firstId : secondId;
    ReservationRecord reservation;
    QString error;
    QVERIFY(ReservationRepository(&m_database).findActiveByUser(winner, &reservation, &error));
    QVERIFY(ReservationService(&m_database).cancel(winner, {{"reservationId", double(reservation.id)}}).succeeded());
}

void ServiceTest::simultaneousDirectOrderAndReservation()
{
    const auto userId = createUser("13800138217");
    QVERIFY(UserService(&m_database).recharge(userId, {{"amountCents", 100}, {"transactionId", "reserve-order-parallel"}}).succeeded());
    for (int iteration = 0; iteration < 32; ++iteration) {
        QSemaphore ready, start;
        auto request = [&](bool order) {
            m_database.database();
            ready.release();
            start.acquire();
            qint64 id = 0;
            QString error;
            const bool succeeded = order
                ? OrderRepository(&m_database).createChargingOrder(QString("parallel-order-%1").arg(iteration), userId, 1, 0, &id, &error)
                : ReservationRepository(&m_database).create(userId, 2, QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODate), &id, &error);
            m_database.releaseCurrentThreadConnection();
            return std::make_pair(succeeded ? id : qint64(0), error);
        };
        auto orderRequest = std::async(std::launch::async, request, true);
        auto reservationRequest = std::async(std::launch::async, request, false);
        ready.acquire(2);
        start.release(2);
        const auto order = orderRequest.get();
        const auto reservation = reservationRequest.get();
        const bool bothSucceeded = order.first && reservation.first;
        if (order.first)
            QVERIFY(BillingService(&m_database).settle(order.first, 0, 0, "completed", "user_stop").succeeded());
        if (reservation.first)
            QVERIFY(ReservationService(&m_database).cancel(userId, {{"reservationId", double(reservation.first)}}).succeeded());
        QVERIFY(!bothSucceeded);
        QVERIFY(order.first || reservation.first);
        QCOMPARE(order.first ? reservation.second : order.second,
                 order.first ? QString("order_conflict") : QString("reservation_conflict"));
    }
}

void ServiceTest::chargingProgressAndIdempotentStop()
{
    const auto user = createUser("13800138301");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000}, {"transactionId", "charging-progress"}}).succeeded());
    const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(reservation.succeeded());
    QDateTime now;
    ChargingService service(&m_database, [&] { return now; });
    const QJsonObject request{{"reservationId", reservation.payload.value("reservationId")}};
    const auto started = service.start(user, request);
    QVERIFY(started.succeeded());
    QCOMPARE(service.start(user, request).payload.value("orderId"), started.payload.value("orderId"));
    now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(61);
    auto events = service.tick(now);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().type, Charging::MessageType::ChargingProgressPush);
    QCOMPARE(events.first().userId, user);
    QCOMPARE(events.first().payload.value("durationSec").toInt(), 61);
    QCOMPARE(events.first().payload.value("energyWh").toInt(), 1016);
    QCOMPARE(events.first().payload.value("feeCents").toInt(), 121);
    QCOMPARE(events.first().payload.value("seq").toInt(), 1);
    events = service.tick(now);
    QVERIFY(events.isEmpty());
    now = now.addSecs(1);
    const QJsonObject stop{{"orderId", started.payload.value("orderId")}};
    const auto stopped = service.stop(user, Charging::Role::User, stop);
    QVERIFY(stopped.succeeded());
    QCOMPARE(stopped.payload.value("durationSec").toInt(), 62);
    QCOMPARE(stopped.payload.value("energyWh").toInt(), 1033);
    QCOMPARE(stopped.payload.value("feeCents").toInt(), 123);
    QCOMPARE(stopped.payload.value("balanceCents").toInt(), 9877);
    QCOMPARE(service.stop(user, Charging::Role::User, stop).payload.value("feeCents"), stopped.payload.value("feeCents"));
    events = service.tick(now);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().type, Charging::MessageType::ChargingStoppedPush);
    QCOMPARE(events.first().payload.value("seq").toInt(), 2);
    QVERIFY(service.tick(now.addSecs(1)).isEmpty());
}

void ServiceTest::chargingAutomaticStops()
{
    // Missing any stop condition leaves a charging row, or emits no final event.
    const QStringList reasons{"insufficient_balance", "fully_charged", "device_fault", "device_offline",
                              "over_temperature", "over_current", "connection_lost"};
    for (int i = 0; i < reasons.size(); ++i) {
        const auto user = createUser(QString("1380013831%1").arg(i));
        QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", i == 0 ? 100 : 10000},
            {"transactionId", QString("charging-auto-%1").arg(i)}}).succeeded());
        const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
        QVERIFY(reservation.succeeded());
        ChargingSensorSample sample;
        QDateTime now;
        ChargingService service(&m_database, [&] { return now; }, [&](const PileRecord &, const QDateTime &) { return sample; });
        const auto started = service.start(user, {{"reservationId", reservation.payload.value("reservationId")}});
        QVERIFY(started.succeeded());
        now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
        if (i == 1) sample.capacityWh = 500;
        QString error;
        if (i == 2 || i == 3) QVERIFY(PileRepository(&m_database).updateStatus(1, "charging", i == 2 ? "fault" : "offline", &error));
        if (i == 4) sample.temperatureC = 60.1;
        if (i == 5) sample.currentA = 150.1; // 60 kW / 400 V = 150 A.
        if (i == 6) sample.connected = false;
        const auto events = service.tick(now);
        QCOMPARE(events.size(), i >= 2 ? 2 : 1);
        if (i >= 2) QCOMPARE(events.first().type, Charging::MessageType::AlarmPush);
        const auto final = events.last();
        QCOMPARE(final.type, Charging::MessageType::ChargingStoppedPush);
        QCOMPARE(final.payload.value("stopReason").toString(), reasons.at(i));
        QCOMPARE(final.payload.value("status").toString(), i >= 2 ? QString("fault_stopped") : QString("completed"));
        QVERIFY(final.payload.value("balanceCents").toInt() >= 0);
        if (i == 0) QCOMPARE(final.payload.value("feeCents").toInt(), 100);
        if (i == 1) QCOMPARE(final.payload.value("energyWh").toInt(), 500);
        QVERIFY(service.tick(now.addSecs(1)).isEmpty());
    }
}

void ServiceTest::chargingSettlementRetryAndAlarmDeduplication()
{
    const auto user = createUser("13800138302");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000}, {"transactionId", "charging-retry"}}).succeeded());
    const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
    ChargingSensorSample sample;
    sample.temperatureC = 61;
    QDateTime now;
    ChargingService service(&m_database, [&] { return now; }, [&](const PileRecord &, const QDateTime &) { return sample; });
    const auto started = service.start(user, {{"reservationId", reservation.payload.value("reservationId")}});
    QVERIFY(started.succeeded());
    now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER fail_charging_settle BEFORE INSERT ON wallet_records WHEN NEW.record_type='charge_payment' BEGIN SELECT RAISE(ABORT,'injected settlement failure'); END"));
    auto events = service.tick(now);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().type, Charging::MessageType::AlarmPush);
    QVERIFY(service.tick(now.addSecs(5)).isEmpty());
    OrderRecord order;
    QString error;
    QVERIFY(OrderRepository(&m_database).findActiveByUser(user, &order, &error));
    QVERIFY(order.id);
    // A new process must recognize the already-open alarm too.
    AlarmService alarms(&m_database);
    const auto duplicate = alarms.raiseOnce(order.pileId, order.id, "over_temperature", "critical", "hot");
    QVERIFY(duplicate.succeeded());
    QCOMPARE(duplicate.payload.value("created").toBool(), false);
    QVERIFY(query.exec("DROP TRIGGER fail_charging_settle"));
    events = service.tick(now.addSecs(10));
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().type, Charging::MessageType::ChargingStoppedPush);
    QCOMPARE(events.first().payload.value("durationSec").toInt(), 60);
    QCOMPARE(events.first().payload.value("feeCents").toInt(), 120);
    QVERIFY(service.tick(now.addSecs(11)).isEmpty());
}

void ServiceTest::chargingRestoreAndPersistedProgress()
{
    const auto user = createUser("13800138303");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000}, {"transactionId", "charging-restore"}}).succeeded());
    qint64 id = 0;
    QString error;
    QVERIFY(OrderRepository(&m_database).createChargingOrder("restore-progress", user, 1, 0, &id, &error));
    QVERIFY(OrderRepository(&m_database).updateProgress(id, 30, 500, 60, &error));
    OrderRecord order;
    QVERIFY(OrderRepository(&m_database).findById(id, &order, &error));
    auto now = QDateTime::fromString(order.startedAt, Qt::ISODate).addSecs(60);
    ChargingService service(&m_database, [&] { return now; });
    QVERIFY(service.restore().succeeded());
    auto events = service.tick(now);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().payload.value("energyWh").toInt(), 1000);
    QVERIFY(OrderRepository(&m_database).findById(id, &order, &error));
    QCOMPARE(order.energyWh, qint64(1000));
    QVERIFY(service.tick(now.addSecs(-50)).isEmpty());
    QVERIFY(PileRepository(&m_database).updateStatus(1, "charging", "offline", &error));
    ChargingService restarted(&m_database, [&] { return now; });
    QVERIFY(restarted.restore().succeeded());
    events = restarted.tick(now);
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.last().payload.value("stopReason").toString(), QString("connection_lost"));
    QVERIFY(restarted.tick(now).isEmpty());
}

void ServiceTest::chargingValidationAndOwnership()
{
    ChargingService service(&m_database);
    QCOMPARE(service.start(1, {}).error, ErrorCode::ValidationFailed);
    QCOMPARE(service.stop(1, Charging::Role::Anonymous, {{"orderId", 1}}).error, ErrorCode::Forbidden);
    QCOMPARE(service.start(1, {{"reservationId", 999999}}).error, ErrorCode::NotFound);
    const auto user = createUser("13800138304");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 100}, {"transactionId", "charging-permission"}}).succeeded());
    const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
    const QJsonObject request{{"reservationId", reservation.payload.value("reservationId")}};
    QCOMPARE(service.start(user + 100, request).error, ErrorCode::Forbidden);
    const auto started = service.start(user, request);
    QVERIFY(started.succeeded());
    const QJsonObject stop{{"orderId", started.payload.value("orderId")}};
    QCOMPARE(service.stop(user + 100, Charging::Role::User, stop).error, ErrorCode::Forbidden);
    QVERIFY(service.stop(1, Charging::Role::Administrator, stop).succeeded());
    ChargingService unavailable;
    QCOMPARE(unavailable.restore().error, ErrorCode::DatabaseError);
    QCOMPARE(unavailable.start(1, {{"reservationId", 1}}).error, ErrorCode::DatabaseError);
}

void ServiceTest::chargingStartRechecksEligibilityAtWrite()
{
    const auto user = createUser("13800138305");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 100}, {"transactionId", "charging-start-race"}}).succeeded());
    const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(reservation.succeeded());
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER drain_at_start AFTER UPDATE OF status ON charging_piles WHEN NEW.id=1 AND NEW.status='charging' BEGIN UPDATE users SET balance_cents=0 WHERE id=" + QString::number(user) + "; END"));
    const auto started = ChargingService(&m_database).start(user, {{"reservationId", reservation.payload.value("reservationId")}});
    QVERIFY(query.exec("DROP TRIGGER drain_at_start"));
    QCOMPARE(started.error, ErrorCode::Conflict);
    QCOMPARE(started.reason, QString("insufficient_balance"));
    PileRecord pile;
    QString error;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("reserved"));
    QVERIFY(ReservationService(&m_database).cancel(user, {{"reservationId", reservation.payload.value("reservationId")}}).succeeded());
}

void ServiceTest::chargingRestoreUnavailablePile()
{
    const auto user = createUser("13800138306");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000}, {"transactionId", "charging-restore-disabled"}}).succeeded());
    qint64 id = 0;
    QString error;
    QVERIFY(OrderRepository(&m_database).createChargingOrder("restore-disabled", user, 1, 0, &id, &error));
    QVERIFY(PileRepository(&m_database).updateStatus(1, "charging", "disabled", &error));
    OrderRecord order;
    QVERIFY(OrderRepository(&m_database).findById(id, &order, &error));
    const auto now = QDateTime::fromString(order.startedAt, Qt::ISODate).addSecs(60);
    ChargingService service(&m_database, [&] { return now; });
    QVERIFY(service.restore().succeeded());
    const auto events = service.tick(now);
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.last().payload.value("status").toString(), QString("fault_stopped"));
    PileRecord pile;
    QVERIFY(PileRepository(&m_database).findById(1, &pile, &error));
    QCOMPARE(pile.status, QString("disabled"));
    QVERIFY(PileRepository(&m_database).updateStatus(1, "disabled", "idle", &error));
}

void ServiceTest::chargingProgressFailureDoesNotPublish()
{
    const auto user = createUser("13800138307");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000}, {"transactionId", "charging-progress-failure"}}).succeeded());
    const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
    QDateTime now;
    ChargingService service(&m_database, [&] { return now; }, [](const PileRecord &, const QDateTime &) {
        ChargingSensorSample sample;
        sample.temperatureC = 60;
        sample.currentA = 150;
        return sample;
    });
    const auto started = service.start(user, {{"reservationId", reservation.payload.value("reservationId")}});
    QVERIFY(started.succeeded());
    now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER fail_progress BEFORE UPDATE OF energy_wh ON charging_orders BEGIN SELECT RAISE(ABORT,'progress failed'); END"));
    QVERIFY(service.tick(now).isEmpty());
    QVERIFY(query.exec("DROP TRIGGER fail_progress"));
    const auto events = service.tick(now);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().type, Charging::MessageType::ChargingProgressPush);
    QCOMPARE(events.first().payload.value("seq").toInt(), 1);
    QCOMPARE(events.first().payload.value("energyWh").toInt(), 1000);
    QVERIFY(service.stop(user, Charging::Role::User, {{"orderId", started.payload.value("orderId")}}).succeeded());
}

void ServiceTest::chargingSequenceSurvivesRestart()
{
    const auto user = createUser("13800138308");
    QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000}, {"transactionId", "charging-sequence"}}).succeeded());
    const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
    qint64 orderId;
    QDateTime now;
    {
        ChargingService first(&m_database, [&] { return now; });
        const auto started = first.start(user, {{"reservationId", reservation.payload.value("reservationId")}});
        QVERIFY(started.succeeded());
        orderId = qint64(started.payload.value("orderId").toDouble());
        now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
        const auto events = first.tick(now);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().payload.value("seq").toInt(), 1);
    }
    for (int expected = 2; expected <= 4; ++expected) {
        DatabaseManager reopened;
        QString error;
        QVERIFY(reopened.open(m_database.databasePath(), &error));
        ChargingService restarted(&reopened, [&] { return now; });
        QVERIFY(restarted.restore().succeeded());
        QVERIFY(restarted.tick(now).isEmpty());
        QVERIFY(restarted.tick(now.addSecs(-1)).isEmpty());
        now = now.addSecs(1);
        const auto events = restarted.tick(now);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().payload.value("seq").toInt(), expected);
    }
    ChargingService final(&m_database, [&] { return now; });
    QVERIFY(final.restore().succeeded());
    QSqlQuery query(m_database.database());
    QVERIFY(query.exec("CREATE TRIGGER fail_sequence BEFORE UPDATE OF push_seq ON charging_orders BEGIN SELECT RAISE(ABORT,'sequence failed'); END"));
    QVERIFY(final.tick(now.addSecs(1)).isEmpty());
    QVERIFY(query.exec("SELECT push_seq,energy_wh FROM charging_orders WHERE id=" + QString::number(orderId)));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 4);
    QCOMPARE(query.value(1).toInt(), 1050);
    query.finish();
    const QJsonObject stop{{"orderId", double(orderId)}};
    QCOMPARE(final.stop(user, Charging::Role::User, stop).error, ErrorCode::DatabaseError);
    QVERIFY(query.exec("DROP TRIGGER fail_sequence"));
    const auto stopped = final.stop(user, Charging::Role::User, stop);
    QVERIFY(stopped.succeeded());
    QCOMPARE(stopped.payload.value("seq").toInt(), 5);
    const auto events = final.tick(now);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().payload.value("seq").toInt(), 5);
    ChargingService afterStop(&m_database);
    QCOMPARE(afterStop.stop(user, Charging::Role::User, stop).payload.value("seq").toInt(), 5);
}

void ServiceTest::chargingNormalStopRetryAfterPileFailure()
{
    for (int i = 0; i < 4; ++i) {
        const auto user = createUser(QString("1380013834%1").arg(i));
        QVERIFY(UserService(&m_database).recharge(user, {{"amountCents", 10000},
            {"transactionId", QString("charging-late-fault-%1").arg(i)}}).succeeded());
        const auto reservation = ReservationService(&m_database).create(user, {{"stationId", 1}, {"pileId", 1}});
        QDateTime now;
        ChargingService service(&m_database, [&] { return now; });
        const auto started = service.start(user, {{"reservationId", reservation.payload.value("reservationId")}});
        QVERIFY(started.succeeded());
        const auto orderId = qint64(started.payload.value("orderId").toDouble());
        const QJsonObject stop{{"orderId", double(orderId)}};
        now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
        QSqlQuery query(m_database.database());
        QVERIFY(query.exec("CREATE TRIGGER fail_late_fault BEFORE INSERT ON wallet_records WHEN NEW.record_type='charge_payment' BEGIN SELECT RAISE(ABORT,'temporary failure'); END"));
        QCOMPARE(service.stop(user, i < 2 ? Charging::Role::User : Charging::Role::Administrator, stop).error, ErrorCode::DatabaseError);
        QString error;
        QVERIFY(PileRepository(&m_database).updateStatus(1, "charging", i % 2 ? "fault" : "offline", &error));
        auto events = service.tick(now.addSecs(30));
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().type, Charging::MessageType::AlarmPush);
        QVERIFY(service.tick(now.addSecs(40)).isEmpty());
        QVERIFY(query.exec("DROP TRIGGER fail_late_fault"));
        events = service.tick(now.addSecs(50));
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().type, Charging::MessageType::ChargingStoppedPush);
        const auto payload = events.first().payload;
        QCOMPARE(payload.value("status").toString(), QString("fault_stopped"));
        QCOMPARE(payload.value("stopReason").toString(), i % 2 ? QString("device_fault") : QString("device_offline"));
        QCOMPARE(payload.value("durationSec").toInt(), 60);
        QCOMPARE(payload.value("energyWh").toInt(), 1000);
        QCOMPARE(payload.value("feeCents").toInt(), 120);
        QCOMPARE(payload.value("balanceCents").toInt(), 9880);
        QVERIFY(service.tick(now.addSecs(60)).isEmpty());
    }
}

namespace
{
struct AdminFixture
{
    QTemporaryDir directory;
    DatabaseManager database;
    QString error;
    bool open()
    {
        if (!database.open(directory.filePath("admin.db"), &error))
            return false;
        QSqlQuery query(database.database());
        return query.exec(
            "INSERT INTO admins(id,username,password_hash,password_salt) VALUES(1,'test','hash','salt')");
    }
    qint64 user(const QString &phone = "13800138901")
    {
        UserRecord record;
        bool created;
        if (!UserRepository(&database).findOrCreate(phone, &record, &created, &error))
            return 0;
        if (!UserService(&database)
                 .recharge(record.id, {{"amountCents", 10000}, {"transactionId", phone}})
                 .succeeded())
            return 0;
        return record.id;
    }
};
} // namespace

void ServiceTest::adminQueriesStatisticsAndExport()
{
    AdminFixture f;
    QVERIFY(f.open());
    const auto user = f.user();
    QVERIFY(user > 0);
    OrderRepository orders(&f.database);
    qint64 first, second, balance;
    QVERIFY(orders.createChargingOrder("admin,\"first\"", user, 1, 0, &first, &f.error));
    QVERIFY(orders.stopAndSettle(first, 60, 1000, 120, "completed", "user_stop", &balance, &f.error));
    QVERIFY(orders.createChargingOrder("admin-second", user, 1, 0, &second, &f.error));
    QVERIFY(
        orders.stopAndSettle(second, 120, 2000, 240, "fault_stopped", "device_fault", &balance, &f.error));
    AdminService admin(&f.database);
    auto summary = admin.execute(1, 1, {{"action", "dashboard.summary"}}, nullptr);
    QVERIFY(summary.succeeded());
    QCOMPARE(summary.payload.value("totalRevenueCents").toInt(), 360);
    QCOMPARE(summary.payload.value("totalOrderCount").toInt(), 2);
    QCOMPARE(summary.payload.value("pileStatus").toObject().value("total").toInt(), 3);
    QCOMPARE(summary.payload.value("revenueMetrics").toObject().value("todayRevenueCents").toInt(), 360);
    QCOMPARE(StatisticsService(&f.database).summary({}).payload.value("totalRevenueCents").toInt(), 360);
    auto trend = admin.execute(1, 2, {{"action", "revenue.trend"}, {"days", 7}}, nullptr);
    QVERIFY(trend.succeeded());
    QCOMPARE(trend.payload.value("points").toArray().size(), 7);
    QCOMPARE(trend.payload.value("points").toArray().last().toObject().value("revenueCents").toInt(), 360);
    QCOMPARE(
        admin.execute(1, 3, {{"action", "dashboard.revenue"}, {"days", 7}}, nullptr).payload.value("points"),
        trend.payload.value("points"));
    for (const auto &action : QStringList{"station.list", "pile.list", "user.list", "order.list",
                                          "alarm.list", "charging.active.list", "stations.list", "piles.list",
                                          "users.list", "orders.list", "alarms.list"})
    {
        auto result = admin.execute(1, 4, {{"action", action}, {"page", 1}, {"pageSize", 1}}, nullptr);
        QVERIFY2(result.succeeded(), qPrintable(action));
        QCOMPARE(result.payload.value("page").toInt(), 1);
        QCOMPARE(result.payload.value("pageSize").toInt(), 1);
        QVERIFY(result.payload.contains("total"));
        QVERIFY(result.payload.value("items").toArray().size() <= 1);
    }
    auto page = admin.execute(1, 5, {{"action", "order.list"}, {"page", 2}, {"pageSize", 1}}, nullptr);
    QCOMPARE(page.payload.value("total").toInt(), 2);
    QCOMPARE(page.payload.value("items").toArray().first().toObject().value("orderId").toInt(), int(first));
    auto filtered = admin.execute(
        1, 6,
        {{"action", "orders.list"}, {"orderNo", "second"}, {"phone", "8901"}, {"status", "fault_stopped"}},
        nullptr);
    QCOMPARE(filtered.payload.value("total").toInt(), 1);
    QCOMPARE(filtered.payload.value("items").toArray().first().toObject().value("stationName").toString(),
             QString("软件园充电站"));
    QCOMPARE(admin.execute(1, 7, {{"action", "pile.status.summary"}}, nullptr).payload.value("total").toInt(),
             3);
    const auto exported = admin.execute(1, 8, {{"action", "report.export"}}, nullptr);
    QVERIFY(exported.succeeded());
    QVERIFY(exported.payload.value("filename").toString().endsWith(".csv"));
    const QByteArray bytes = exported.payload.value("content").toString().toUtf8();
    QVERIFY(bytes.startsWith(QByteArray::fromHex("efbbbf")));
    QVERIFY(bytes.contains("\"admin,\"\"first\"\"\""));
    QVERIFY(bytes.contains("admin-second"));
    QCOMPARE(exported.payload.value("total").toInt(), 2);
    QCOMPARE(admin.execute(1, 9, {{"action", "stations.list"}, {"keyword", "星海"}}, nullptr)
                 .payload.value("total")
                 .toInt(),
             1);
    QCOMPARE(
        admin.execute(1, 10, {{"action", "piles.list"}, {"stationId", 0}, {"status", "available"}}, nullptr)
            .payload.value("total")
            .toInt(),
        3);
}

void ServiceTest::adminMaintenanceAndAliases()
{
    AdminFixture f;
    QVERIFY(f.open());
    AdminService admin(&f.database);
    QJsonObject station{{"action", "station.create"}, {"name", "测试站"}, {"address", "测试地址"},
                        {"longitude", 121.5},         {"latitude", 38.8}, {"priceCentsPerKwh", 150},
                        {"status", "online"}};
    const auto created = admin.execute(1, 1, station, nullptr);
    QVERIFY(created.succeeded());
    const auto id = created.payload.value("stationId");
    QCOMPARE(admin.execute(1, 2, {{"action", "station.detail"}, {"stationId", id}}, nullptr)
                 .payload.value("name")
                 .toString(),
             QString("测试站"));
    QCOMPARE(admin.execute(1, 3, {{"action", "stations.detail"}, {"stationId", id}}, nullptr)
                 .payload.value("name")
                 .toString(),
             QString("测试站"));
    QVERIFY(
        admin.execute(1, 4, {{"action", "station.update"}, {"stationId", id}, {"name", "新站名"}}, nullptr)
            .succeeded());
    QVERIFY(admin
                .execute(1, 5, {{"action", "stations.update"}, {"stationId", id}, {"priceCentsPerKwh", 200}},
                         nullptr)
                .succeeded());
    StationRecord saved;
    QVERIFY(StationRepository(&f.database).findById(qint64(id.toDouble()), &saved, &f.error));
    QCOMPARE(saved.name, QString("新站名"));
    QCOMPARE(saved.priceCentsPerKwh, 200);
    station.insert("action", "stations.create");
    QVERIFY(admin.execute(1, 6, station, nullptr).succeeded());
    QCOMPARE(admin.execute(1, 7, {{"action", "piles.detail"}, {"pileId", 1}}, nullptr)
                 .payload.value("pileCode")
                 .toString(),
             QString("DL-SP-001"));
    AlarmRecord alarm;
    alarm.pileId = 1;
    alarm.alarmType = "temperature_high";
    alarm.severity = "critical";
    alarm.message = "温度过高";
    qint64 alarmId;
    QVERIFY(AlarmRepository(&f.database).insert(alarm, &alarmId, &f.error));
    auto detail = admin.execute(1, 8, {{"action", "alarms.detail"}, {"alarmId", double(alarmId)}}, nullptr);
    QCOMPARE(detail.payload.value("message").toString(), QString("温度过高"));
    QVERIFY(admin
                .execute(1, 9,
                         {{"action", "alarm.handle"}, {"alarmId", double(alarmId)}, {"status", "resolved"}},
                         nullptr)
                .succeeded());
    auto resolved = admin.execute(
        1, 10, {{"action", "alarm.list"}, {"status", "resolved"}, {"severity", "critical"}}, nullptr);
    QCOMPARE(resolved.payload.value("total").toInt(), 1);
    QCOMPARE(resolved.payload.value("items").toArray().first().toObject().value("handledByAdminId").toInt(),
             1);
    QVERIFY(resolved.payload.value("items")
                .toArray()
                .first()
                .toObject()
                .value("recoveredAt")
                .toString()
                .endsWith('Z'));
    QVERIFY(admin
                .execute(1, 11,
                         {{"action", "alarm.handle"}, {"alarmId", double(alarmId)}, {"status", "resolved"}},
                         nullptr)
                .succeeded());
}

void ServiceTest::adminControlConflictsAndAudits()
{
    AdminFixture f;
    QVERIFY(f.open());
    AdminService admin(&f.database);
    const auto user = f.user();
    const auto reserved = ReservationService(&f.database).create(user, {{"stationId", 1}, {"pileId", 1}});
    QVERIFY(reserved.succeeded());
    QCOMPARE(admin.execute(1, 1, {{"action", "pile.disable"}, {"pileId", 1}}, nullptr).reason,
             QString("command_conflict"));
    // Even if a device reports idle, the persisted active reservation forbids disabling it.
    QVERIFY(PileRepository(&f.database).updateStatus(1, "reserved", "idle", &f.error));
    QCOMPARE(admin.execute(1, 2, {{"action", "pile.disable"}, {"pileId", 1}}, nullptr).error,
             ErrorCode::Conflict);
    QVERIFY(ReservationService(&f.database)
                .cancel(user, {{"reservationId", reserved.payload.value("reservationId")}})
                .succeeded());
    QVERIFY(admin.execute(1, 3, {{"action", "pile.disable"}, {"pileId", 1}}, nullptr).succeeded());
    QVERIFY(admin.execute(1, 4, {{"action", "pile.enable"}, {"pileId", 1}}, nullptr).succeeded());
    auto restart = admin.execute(1, 5, {{"action", "pile.restart"}, {"pileId", 1}}, nullptr);
    QVERIFY(restart.succeeded());
    QVERIFY(!restart.payload.value("lastHeartbeatAt").toString().isEmpty());
    QVERIFY(admin.execute(1, 6, {{"action", "piles.control"}, {"pileId", 1}, {"command", "disable"}}, nullptr)
                .succeeded());
    QVERIFY(admin.execute(1, 7, {{"action", "piles.control"}, {"pileId", 1}, {"command", "enable"}}, nullptr)
                .succeeded());
    QCOMPARE(admin.execute(1, 8, {{"action", "pile.restart"}, {"pileId", 99999}}, nullptr).error,
             ErrorCode::NotFound);
    QCOMPARE(admin.execute(1, 9, {{"action", "pile.enable"}, {"pileId", "bad"}}, nullptr).error,
             ErrorCode::ValidationFailed);
    QSqlQuery query(f.database.database());
    QVERIFY(query.exec(
        "SELECT COUNT(*),SUM(result='success'),SUM(result='failure') FROM device_control_records"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 9);
    QCOMPARE(query.value(1).toInt(), 5);
    QCOMPARE(query.value(2).toInt(), 4);
    query.finish();
    QVERIFY(query.exec(
        "SELECT admin_id,pile_id,request_id,detail FROM device_control_records WHERE request_id=8"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(query.value(1).isNull());
    QCOMPARE(query.value(2).toInt(), 8);
    QVERIFY(query.value(3).toString().contains("99999"));
}

void ServiceTest::adminRemoteStopAndFreeze()
{
    AdminFixture f;
    QVERIFY(f.open());
    const auto user = f.user();
    const auto reserved = ReservationService(&f.database).create(user, {{"stationId", 1}, {"pileId", 1}});
    QDateTime now;
    ChargingService charging(&f.database, [&] { return now; });
    const auto started = charging.start(user, {{"reservationId", reserved.payload.value("reservationId")}});
    QVERIFY(started.succeeded());
    now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
    const auto orderId = started.payload.value("orderId");
    AdminService admin(&f.database);
    QVERIFY(
        admin.execute(1, 1, {{"action", "user.freeze"}, {"userId", double(user)}}, &charging).succeeded());
    QCOMPARE(OrderService(&f.database).active(user).payload.value("status").toString(), QString("charging"));
    QCOMPARE(admin.execute(1, 2, {{"action", "pile.restart"}, {"pileId", 1}}, &charging).error,
             ErrorCode::Conflict);
    QCOMPARE(admin.execute(1, 3, {{"action", "pile.disable"}, {"pileId", 1}}, &charging).error,
             ErrorCode::Conflict);
    // A stale device status must not let an active order through the guard.
    QVERIFY(PileRepository(&f.database).updateStatus(1, "charging", "idle", &f.error));
    QCOMPARE(admin.execute(1, 4, {{"action", "pile.restart"}, {"pileId", 1}}, &charging).error,
             ErrorCode::Conflict);
    QVERIFY(PileRepository(&f.database).updateStatus(1, "idle", "charging", &f.error));
    auto stopped =
        admin.execute(1, 5, {{"action", "pile.stop"}, {"pileId", 1}, {"orderId", orderId}}, &charging);
    QVERIFY(stopped.succeeded());
    QCOMPARE(stopped.payload.value("feeCents").toInt(), 120);
    QCOMPARE(stopped.payload.value("balanceCents").toInt(), 9880);
    auto repeated =
        admin.execute(1, 5, {{"action", "pile.stop"}, {"pileId", 1}, {"orderId", orderId}}, &charging);
    QCOMPARE(repeated.payload.value("feeCents").toInt(), 120);
    QVERIFY(admin.execute(1, 6, {{"action", "charging.stop"}, {"orderId", orderId}}, &charging).succeeded());
    QCOMPARE(ReservationService(&f.database).create(user, {{"stationId", 1}, {"pileId", 1}}).reason,
             QString("user_frozen"));
    QVERIFY(
        admin.execute(1, 7, {{"action", "user.unfreeze"}, {"userId", double(user)}}, &charging).succeeded());
    QVERIFY(admin
                .execute(1, 8, {{"action", "users.freeze"}, {"userId", double(user)}, {"frozen", true}},
                         &charging)
                .succeeded());
    QVERIFY(admin
                .execute(1, 9, {{"action", "users.freeze"}, {"userId", double(user)}, {"frozen", false}},
                         &charging)
                .succeeded());
    UserRecord finalUser;
    QVERIFY(UserRepository(&f.database).findById(user, &finalUser, &f.error));
    QCOMPARE(finalUser.status, QString("normal"));
    QSqlQuery query(f.database.database());
    QVERIFY(query.exec("SELECT COUNT(*) FROM wallet_records WHERE record_type='charge_payment'"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    query.finish();
    QVERIFY(query.exec(
        "SELECT COUNT(*) FROM device_control_records WHERE command_type='pile.stop' AND order_id=" +
        QString::number(qint64(orderId.toDouble()))));
    QVERIFY(query.next());
    QVERIFY(query.value(0).toInt() >= 2);
}

void ServiceTest::adminValidationAndDatabaseFailures()
{
    AdminFixture f;
    QVERIFY(f.open());
    AdminService admin(&f.database);
    QCOMPARE(admin.execute(0, 1, {{"action", "dashboard.summary"}}, nullptr).error, ErrorCode::Unauthorized);
    QCOMPARE(admin.execute(1, 1, {{"action", "unknown"}}, nullptr).error, ErrorCode::UnsupportedMessage);
    for (const auto &action : QStringList{"station.list", "pile.list", "alarm.list", "user.list",
                                          "order.list", "charging.active.list"})
        for (const auto &invalidPage : QList<QJsonObject>{{{"page", 0}},
                                                          {{"page", 1.5}},
                                                          {{"pageSize", 0}},
                                                          {{"pageSize", 101}},
                                                          {{"pageSize", "20"}}})
        {
            auto payload = invalidPage;
            payload.insert("action", action);
            QCOMPARE(admin.execute(1, 1, payload, nullptr).error, ErrorCode::ValidationFailed);
        }
    for (const auto &payload :
         QList<QJsonObject>{{{"action", "station.create"}, {"name", "missing"}},
                            {{"action", "station.update"}, {"stationId", 1}, {"longitude", 181}},
                            {{"action", "user.freeze"}, {"userId", 0}},
                            {{"action", "revenue.trend"}, {"days", 0}},
                            {{"action", "order.list"}, {"from", "2026-99-01"}},
                            {{"action", "alarm.handle"}, {"alarmId", 1}, {"status", "bad"}}})
        QCOMPARE(admin.execute(1, 1, payload, nullptr).error, ErrorCode::ValidationFailed);
    for (const auto &payload :
         QList<QJsonObject>{{{"action", "station.detail"}, {"stationId", 99999}},
                            {{"action", "station.update"}, {"stationId", 99999}, {"name", "abc"}},
                            {{"action", "user.freeze"}, {"userId", 99999}},
                            {{"action", "alarm.handle"}, {"alarmId", 99999}, {"status", "resolved"}}})
        QCOMPARE(admin.execute(1, 1, payload, nullptr).error, ErrorCode::NotFound);
    QCOMPARE(AdminService().execute(1, 1, {{"action", "station.list"}}, nullptr).error,
             ErrorCode::DatabaseError);
    QCOMPARE(StatisticsService().summary({}).error, ErrorCode::DatabaseError);
    QSqlQuery query(f.database.database());
    QVERIFY(query.exec("CREATE TRIGGER reject_admin_control BEFORE UPDATE ON charging_piles BEGIN SELECT "
                       "RAISE(ABORT,'control failure'); END"));
    QCOMPARE(admin.execute(1, 22, {{"action", "pile.disable"}, {"pileId", 1}}, nullptr).error,
             ErrorCode::DatabaseError);
    QVERIFY(query.exec("DROP TRIGGER reject_admin_control"));
    QVERIFY(query.exec("SELECT result FROM device_control_records WHERE request_id=22"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("failure"));
    query.finish();
    QVERIFY(query.exec("CREATE TRIGGER reject_admin_audit BEFORE INSERT ON device_control_records BEGIN "
                       "SELECT RAISE(ABORT,'audit failure'); END"));
    QCOMPARE(admin.execute(1, 23, {{"action", "pile.disable"}, {"pileId", 1}}, nullptr).error,
             ErrorCode::DatabaseError);
    QVERIFY(query.exec("DROP TRIGGER reject_admin_audit"));
    PileRecord pile;
    QVERIFY(PileRepository(&f.database).findById(1, &pile, &f.error));
    QCOMPARE(pile.status, QString("idle"));
}

void ServiceTest::adminControlReplayAndClientMetadata()
{
    AdminFixture f;
    QVERIFY(f.open());
    const auto user = f.user();
    AdminService admin(&f.database);
    const auto list = admin.execute(1, 1, {{"action", "piles.list"}, {"pageSize", 1}, {"page", 2}}, nullptr);
    QCOMPARE(list.payload.value("meta").toObject().value("page").toInt(), 2);
    QCOMPARE(list.payload.value("meta").toObject().value("totalPages").toInt(), 3);
    QCOMPARE(list.payload.value("items").toArray().first().toObject().value("status").toString(),
             QString("idle"));
    QCOMPARE(admin.execute(1, 2, {{"action", "pile.list"}}, nullptr)
                 .payload.value("items")
                 .toArray()
                 .first()
                 .toObject()
                 .value("status")
                 .toString(),
             QString("available"));
    const auto reservation = ReservationService(&f.database).create(user, {{"stationId", 1}, {"pileId", 1}});
    QDateTime now;
    ChargingService charging(&f.database, [&] { return now; });
    auto started = charging.start(user, {{"reservationId", reservation.payload.value("reservationId")}});
    QVERIFY(started.succeeded());
    now = QDateTime::fromString(started.payload.value("startedAt").toString(), Qt::ISODate).addSecs(60);
    const QJsonObject stop{{"action", "piles.control"}, {"pileId", 1}, {"command", "stop"}};
    auto first = admin.execute(1, 3, stop, &charging);
    auto repeated = admin.execute(1, 3, stop, &charging);
    QVERIFY(first.succeeded());
    QVERIFY(repeated.succeeded());
    QCOMPARE(repeated.payload.value("feeCents").toInt(), 120);
    QCOMPARE(repeated.payload.value("orderId"), started.payload.value("orderId"));
    QSqlQuery query(f.database.database());
    QVERIFY(query.exec("SELECT COUNT(*) FROM wallet_records WHERE record_type='charge_payment'"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    query.finish();
    const QJsonObject restart{{"action", "piles.control"}, {"pileId", 1}, {"command", "restart"}};
    QVERIFY(admin.execute(1, 4, restart, &charging).succeeded());
    QVERIFY(query.exec("UPDATE charging_piles SET last_heartbeat_at='2020-01-01T00:00:00Z' WHERE id=1"));
    QVERIFY(admin.execute(1, 4, restart, &charging).succeeded());
    PileRecord pile;
    QVERIFY(PileRepository(&f.database).findById(1, &pile, &f.error));
    QCOMPARE(pile.lastHeartbeatAt, QString("2020-01-01T00:00:00Z"));
    QCOMPARE(
        admin.execute(1, 5, {{"action", "piles.control"}, {"pileId", 1}, {"command", "invalid"}}, &charging)
            .error,
        ErrorCode::ValidationFailed);
    QCOMPARE(admin.execute(1, 6, {{"action", "pile.stop"}, {"pileId", 1}}, &charging).reason,
             QString("order_not_active"));
    auto active = ReservationService(&f.database).create(user, {{"stationId", 1}, {"pileId", 1}});
    started = charging.start(user, {{"reservationId", active.payload.value("reservationId")}});
    QVERIFY(started.succeeded());
    const auto orderId = qint64(started.payload.value("orderId").toDouble());
    QVERIFY(OrderRepository(&f.database).updateProgress(orderId, 30, 500, 60, &f.error));
    auto summary = admin.execute(1, 7, {{"action", "dashboard.summary"}}, &charging);
    QCOMPARE(summary.payload.value("totalOrderCount").toInt(), 2);
    QCOMPARE(summary.payload.value("activeOrderCount").toInt(), 1);
    QCOMPARE(summary.payload.value("totalRevenueCents").toInt(), 120);
    QCOMPARE(
        admin.execute(1, 8, {{"action", "pile.stop"}, {"pileId", 2}, {"orderId", double(orderId)}}, &charging)
            .error,
        ErrorCode::Conflict);
    QCOMPARE(admin.execute(1, 9, {{"action", "pile.stop"}, {"pileId", 1}}, nullptr).error,
             ErrorCode::InternalError);
    QVERIFY(query.exec("SELECT pile_id,order_id FROM device_control_records WHERE request_id=8"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);
    QCOMPARE(query.value(1).toLongLong(), orderId);
}

void ServiceTest::adminAlarmDetailSurvivesListFilters()
{
    AdminFixture f;
    QVERIFY(f.open());
    AlarmRecord alarm;
    alarm.pileId = 1;
    alarm.alarmType = "temperature_high";
    alarm.severity = "critical";
    alarm.message = "过热";
    qint64 id;
    QVERIFY(AlarmRepository(&f.database).insert(alarm, &id, &f.error));
    const auto detail =
        AdminService(&f.database)
            .execute(1, 1, {{"action", "alarms.detail"}, {"alarmId", double(id)}, {"severity", "warning"}},
                     nullptr);
    QVERIFY(detail.succeeded());
    QCOMPARE(detail.payload.value("alarmId").toInt(), int(id));
    QCOMPARE(detail.payload.value("message").toString(), QString("过热"));
}

void ServiceTest::adminControlReplayIsScopedToConnection()
{
    AdminFixture f;
    QVERIFY(f.open());
    AdminService admin(&f.database);
    QVERIFY(admin
                .execute(1, 10,
                         {{"action", "pile.disable"}, {"pileId", 1}, {"_requestScope", "connection-A"}},
                         nullptr)
                .succeeded());
    QVERIFY(admin
                .execute(1, 11, {{"action", "pile.enable"}, {"pileId", 1}, {"_requestScope", "connection-A"}},
                         nullptr)
                .succeeded());
    QVERIFY(admin
                .execute(1, 10,
                         {{"action", "pile.disable"}, {"pileId", 1}, {"_requestScope", "connection-B"}},
                         nullptr)
                .succeeded());
    PileRecord pile;
    QVERIFY(PileRepository(&f.database).findById(1, &pile, &f.error));
    QCOMPARE(pile.status, QString("disabled"));
}

QTEST_GUILESS_MAIN(ServiceTest)
