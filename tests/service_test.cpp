#include "service_test.h"
#include "repositories/userrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/pilerepository.h"
#include "services/userservice.h"
#include "services/stationservice.h"
#include "services/pileservice.h"
#include "services/serviceresult.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QSqlQuery>
#include <QtTest>

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

QTEST_GUILESS_MAIN(ServiceTest)
