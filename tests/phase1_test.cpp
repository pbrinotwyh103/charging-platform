#include "phase1_test.h"

#include "map/tencentmapadapter.h"

#include <QSignalSpy>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QElapsedTimer>
#include <QUrlQuery>
#include <QtTest>

void Phase1Test::initTestCase()
{
    QVERIFY2(m_temporaryDirectory.isValid(), "无法创建临时测试目录");
    m_databasePath = m_temporaryDirectory.filePath(QStringLiteral("phase1.db"));
    QString error;
    QVERIFY2(m_server.start(0, m_databasePath, &error), qPrintable(error));
    QVERIFY(m_server.listeningPort() != 0);
}

void Phase1Test::connectClient(Charging::ClientConnection &client)
{
    client.setAutoReconnect(false);
    QSignalSpy connected(&client, &Charging::ClientConnection::connected);
    client.connectToServer(QStringLiteral("127.0.0.1"), m_server.listeningPort());
    QTRY_COMPARE_WITH_TIMEOUT(connected.count(), 1, 3'000);
}

Charging::Message Phase1Test::request(Charging::ClientConnection &client,
                                     Charging::MessageType requestType,
                                     Charging::MessageType responseType,
                                     const QJsonObject &payload)
{
    QSignalSpy messages(&client, &Charging::ClientConnection::messageReceived);
    const quint32 requestId = client.nextRequestId();
    if (!client.send(requestType, requestId, payload)) {
        QTest::qFail("发送测试请求失败", __FILE__, __LINE__);
        return {};
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5'000) {
        if (messages.isEmpty()) {
            messages.wait(5'000 - static_cast<int>(timer.elapsed()));
        }
        while (!messages.isEmpty()) {
            const Charging::Message message = qvariant_cast<Charging::Message>(
                messages.takeFirst().at(0));
            if (message.header.requestId == requestId
                && message.header.messageType == responseType) {
                return message;
            }
        }
    }
    QTest::qFail("未收到匹配的服务器响应", __FILE__, __LINE__);
    return {};
}

void Phase1Test::pingAndUnauthorizedGuard()
{
    Charging::ClientConnection client;
    connectClient(client);
    const Charging::Message pong = request(client, Charging::MessageType::Ping,
                                           Charging::MessageType::Pong);
    QCOMPARE(pong.header.statusCode, Charging::ErrorCode::Success);

    const Charging::Message profile = request(
        client, Charging::MessageType::UserProfileRequest,
        Charging::MessageType::UserProfileResponse);
    QCOMPARE(profile.header.statusCode, Charging::ErrorCode::Unauthorized);
}

void Phase1Test::invalidPhoneIsRejected()
{
    Charging::ClientConnection client;
    connectClient(client);
    const Charging::Message response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("123")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::ValidationFailed);
}

void Phase1Test::userAutoRegistrationAndProfile()
{
    Charging::ClientConnection client;
    connectClient(client);
    const QJsonObject loginPayload = {
        {QStringLiteral("phone"), QStringLiteral("13800138000")}
    };
    Charging::Message response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse, loginPayload);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QVERIFY(response.payload.value(QStringLiteral("created")).toBool());
    QCOMPARE(response.payload.value(QStringLiteral("nickname")).toString(),
             QStringLiteral("用户8000"));
    QCOMPARE(response.payload.value(QStringLiteral("avatarPath")).toString(),
             QStringLiteral("default://gray-avatar"));
    QCOMPARE(response.payload.value(QStringLiteral("balanceCents")).toInt(), 0);
    QVERIFY(!response.payload.value(QStringLiteral("sessionId")).toString().isEmpty());

    response = request(client, Charging::MessageType::UserProfileRequest,
                       Charging::MessageType::UserProfileResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("phone")).toString(),
             QStringLiteral("13800138000"));

    response = request(client, Charging::MessageType::LogoutRequest,
                       Charging::MessageType::LogoutResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    response = request(client, Charging::MessageType::UserLoginRequest,
                       Charging::MessageType::UserLoginResponse, loginPayload);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QVERIFY(!response.payload.value(QStringLiteral("created")).toBool());
}

void Phase1Test::nicknameUpdateIsValidatedAndPersisted()
{
    Charging::ClientConnection unauthenticated;
    connectClient(unauthenticated);
    Charging::Message response = request(
        unauthenticated, Charging::MessageType::UserProfileUpdateRequest,
        Charging::MessageType::UserProfileUpdateResponse,
        {{QStringLiteral("nickname"), QStringLiteral("未登录用户")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Unauthorized);

    Charging::ClientConnection client;
    connectClient(client);
    response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13400134000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    const QString originalNickname =
        response.payload.value(QStringLiteral("nickname")).toString();

    response = request(
        client, Charging::MessageType::UserProfileUpdateRequest,
        Charging::MessageType::UserProfileUpdateResponse,
        {{QStringLiteral("nickname"), QStringLiteral("充电达人")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("nickname")).toString(),
             QStringLiteral("充电达人"));
    QCOMPARE(response.payload.value(QStringLiteral("phone")).toString(),
             QStringLiteral("13400134000"));

    response = request(client, Charging::MessageType::UserProfileRequest,
                       Charging::MessageType::UserProfileResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("nickname")).toString(),
             QStringLiteral("充电达人"));

    response = request(
        client, Charging::MessageType::UserProfileUpdateRequest,
        Charging::MessageType::UserProfileUpdateResponse,
        {{QStringLiteral("nickname"), QStringLiteral("充电达人")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Conflict);
    QVERIFY(response.payload.value(QStringLiteral("message")).toString()
                .contains(QStringLiteral("相同")));

    response = request(
        client, Charging::MessageType::UserProfileUpdateRequest,
        Charging::MessageType::UserProfileUpdateResponse,
        {{QStringLiteral("nickname"), QStringLiteral("非法/昵称")}});
    QCOMPARE(response.header.statusCode,
             Charging::ErrorCode::ValidationFailed);

    response = request(
        client, Charging::MessageType::UserProfileUpdateRequest,
        Charging::MessageType::UserProfileUpdateResponse,
        {{QStringLiteral("nickname"), QStringLiteral("A")}});
    QCOMPARE(response.header.statusCode,
             Charging::ErrorCode::ValidationFailed);

    response = request(client, Charging::MessageType::UserProfileRequest,
                       Charging::MessageType::UserProfileResponse);
    QCOMPARE(response.payload.value(QStringLiteral("nickname")).toString(),
             QStringLiteral("充电达人"));
    QVERIFY(response.payload.value(QStringLiteral("nickname")).toString()
                != originalNickname);
}

void Phase1Test::rechargeUpdatesBalanceAndLedgerImmediately()
{
    Charging::ClientConnection client;
    connectClient(client);
    Charging::Message response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13500135000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("balanceCents")).toInt(), 0);

    const QJsonObject rechargePayload = {
        {QStringLiteral("amountCents"), 12345},
        {QStringLiteral("transactionId"), QStringLiteral("phase1-recharge-1")}
    };
    response = request(client, Charging::MessageType::WalletRechargeRequest,
                       Charging::MessageType::WalletRechargeResponse,
                       rechargePayload);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("amountCents")).toInt(),
             12345);
    QCOMPARE(response.payload.value(QStringLiteral("balanceCents")).toInt(),
             12345);
    QCOMPARE(response.payload.value(QStringLiteral("status")).toString(),
             QStringLiteral("success"));

    response = request(client, Charging::MessageType::UserProfileRequest,
                       Charging::MessageType::UserProfileResponse);
    QCOMPARE(response.payload.value(QStringLiteral("balanceCents")).toInt(),
             12345);

    // A retry with the same simulated-payment transaction must not add money twice.
    response = request(client, Charging::MessageType::WalletRechargeRequest,
                       Charging::MessageType::WalletRechargeResponse,
                       rechargePayload);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("balanceCents")).toInt(),
             12345);

    response = request(client, Charging::MessageType::WalletLedgerRequest,
                       Charging::MessageType::WalletLedgerResponse,
                       {{QStringLiteral("page"), 1},
                        {QStringLiteral("pageSize"), 20}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("total")).toInt(), 1);
    const QJsonArray items =
        response.payload.value(QStringLiteral("items")).toArray();
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().toObject()
                 .value(QStringLiteral("balanceAfterCents")).toInt(), 12345);
}

void Phase1Test::historicalOrdersAreUserScoped()
{
    Charging::ClientConnection client;
    connectClient(client);
    Charging::Message response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13700137000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    const qint64 userId = response.payload.value(QStringLiteral("userId"))
                              .toVariant().toLongLong();
    QVERIFY(userId > 0);

    const QString connectionName = QStringLiteral("order_history_fixture");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          connectionName);
        database.setDatabaseName(m_databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT INTO charging_orders("
            "order_no, user_id, station_id, pile_id, status, started_at, "
            "stopped_at, duration_seconds, energy_wh, unit_price_cents, fee_cents) "
            "VALUES(?, ?, 1, 1, 'completed', ?, ?, 900, 2500, 120, 300)"));
        query.addBindValue(QStringLiteral("ORDER-HISTORY-001"));
        query.addBindValue(userId);
        query.addBindValue(QStringLiteral("2026-09-06T10:00:00+08:00"));
        query.addBindValue(QStringLiteral("2026-09-06T10:15:00+08:00"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const qint64 orderId = query.lastInsertId().toLongLong();
        QVERIFY(orderId > 0);

        query.prepare(QStringLiteral(
            "INSERT INTO wallet_records("
            "record_no, user_id, order_id, record_type, amount_cents, "
            "balance_after_cents, status) "
            "VALUES(?, ?, ?, 'charge_payment', -300, 700, 'success')"));
        query.addBindValue(QStringLiteral("PAY-HISTORY-001"));
        query.addBindValue(userId);
        query.addBindValue(orderId);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

        query.prepare(QStringLiteral(
            "INSERT INTO users(phone, nickname) VALUES(?, ?)"));
        query.addBindValue(QStringLiteral("13600136000"));
        query.addBindValue(QStringLiteral("其他用户"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        const qint64 otherUserId = query.lastInsertId().toLongLong();
        query.prepare(QStringLiteral(
            "INSERT INTO charging_orders("
            "order_no, user_id, station_id, pile_id, status, started_at, "
            "unit_price_cents, fee_cents) "
            "VALUES('ORDER-OTHER-001', ?, 1, 1, 'completed', "
            "'2026-09-05T10:00:00+08:00', 120, 100)"));
        query.addBindValue(otherUserId);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    response = request(client, Charging::MessageType::OrderHistoryRequest,
                       Charging::MessageType::OrderHistoryResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    const QJsonArray items = response.payload.value(QStringLiteral("items")).toArray();
    QCOMPARE(items.size(), 1);
    const QJsonObject order = items.first().toObject();
    QCOMPARE(order.value(QStringLiteral("orderNo")).toString(),
             QStringLiteral("ORDER-HISTORY-001"));
    QCOMPARE(order.value(QStringLiteral("stationName")).toString(),
             QStringLiteral("软件园充电站"));
    QCOMPARE(order.value(QStringLiteral("stationId")).toInt(), 1);
    QCOMPARE(order.value(QStringLiteral("feeCents")).toInt(), 300);
    QCOMPARE(order.value(QStringLiteral("paymentStatus")).toString(),
             QStringLiteral("success"));
}

void Phase1Test::favoritesAreIdempotentAndUserScoped()
{
    Charging::ClientConnection client;
    connectClient(client);
    Charging::Message response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13500135000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);

    response = request(client, Charging::MessageType::StationListRequest,
                       Charging::MessageType::StationListResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QJsonArray stations = response.payload.value(QStringLiteral("items")).toArray();
    QCOMPARE(stations.size(), 2);
    QVERIFY(!stations.first().toObject().value(QStringLiteral("favorited")).toBool());

    const QJsonObject favoritePayload{
        {QStringLiteral("stationId"), 1},
        {QStringLiteral("favorited"), true}
    };
    response = request(client, Charging::MessageType::FavoriteToggleRequest,
                       Charging::MessageType::FavoriteToggleResponse,
                       favoritePayload);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QVERIFY(response.payload.value(QStringLiteral("favorited")).toBool());
    QVERIFY(!response.payload.value(QStringLiteral("updatedAt")).toString().isEmpty());

    // Repeating the same target state is idempotent and must not duplicate data.
    response = request(client, Charging::MessageType::FavoriteToggleRequest,
                       Charging::MessageType::FavoriteToggleResponse,
                       favoritePayload);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);

    response = request(client, Charging::MessageType::FavoriteListRequest,
                       Charging::MessageType::FavoriteListResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QJsonArray favorites = response.payload.value(QStringLiteral("items")).toArray();
    QCOMPARE(favorites.size(), 1);
    QCOMPARE(favorites.first().toObject().value(QStringLiteral("stationId")).toInt(), 1);
    QVERIFY(favorites.first().toObject().value(QStringLiteral("favorited")).toBool());

    response = request(client, Charging::MessageType::StationListRequest,
                       Charging::MessageType::StationListResponse);
    stations = response.payload.value(QStringLiteral("items")).toArray();
    QVERIFY(stations.first().toObject().value(QStringLiteral("favorited")).toBool());

    Charging::ClientConnection otherClient;
    connectClient(otherClient);
    response = request(
        otherClient, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13400134000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    response = request(otherClient, Charging::MessageType::FavoriteListRequest,
                       Charging::MessageType::FavoriteListResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QVERIFY(response.payload.value(QStringLiteral("items")).toArray().isEmpty());

    response = request(
        client, Charging::MessageType::FavoriteToggleRequest,
        Charging::MessageType::FavoriteToggleResponse,
        {{QStringLiteral("stationId"), 1},
         {QStringLiteral("favorited"), false}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QVERIFY(!response.payload.value(QStringLiteral("favorited")).toBool());
    response = request(client, Charging::MessageType::FavoriteListRequest,
                       Charging::MessageType::FavoriteListResponse);
    QVERIFY(response.payload.value(QStringLiteral("items")).toArray().isEmpty());

    response = request(
        client, Charging::MessageType::FavoriteToggleRequest,
        Charging::MessageType::FavoriteToggleResponse,
        {{QStringLiteral("stationId"), 999999},
         {QStringLiteral("favorited"), true}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::NotFound);
}

void Phase1Test::tencentGeocoderContract()
{
    const QUrl url = TencentMapAdapter::buildGeocodeUrl(
        QStringLiteral("上海市南京西路 100 号"),
        QStringLiteral("静安区"), QStringLiteral("test-secret-key"));
    QCOMPARE(url.scheme(), QStringLiteral("https"));
    QCOMPARE(url.host(), QStringLiteral("apis.map.qq.com"));
    QCOMPARE(url.path(), QStringLiteral("/ws/geocoder/v1/"));
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(QStringLiteral("address")),
             QStringLiteral("上海市南京西路 100 号"));
    QCOMPARE(query.queryItemValue(QStringLiteral("region")),
             QStringLiteral("静安区"));
    QCOMPARE(query.queryItemValue(QStringLiteral("output")),
             QStringLiteral("json"));
    QCOMPARE(query.queryItemValue(QStringLiteral("key")),
             QStringLiteral("test-secret-key"));

    const QByteArray successBody = QJsonDocument(QJsonObject{
        {QStringLiteral("status"), 0},
        {QStringLiteral("message"), QStringLiteral("query ok")},
        {QStringLiteral("result"), QJsonObject{
            {QStringLiteral("title"), QStringLiteral("上海市人民广场")},
            {QStringLiteral("location"), QJsonObject{
                {QStringLiteral("lat"), 31.2304},
                {QStringLiteral("lng"), 121.4737}
            }}
        }}
    }).toJson(QJsonDocument::Compact);
    MapGeocodeResult result = TencentMapAdapter::parseGeocodeResponse(
        successBody, QStringLiteral("人民广场"), QStringLiteral("上海市"));
    QCOMPARE(result.error, Charging::ErrorCode::Success);
    QCOMPARE(result.coordinate.value(QStringLiteral("crs")).toString(),
             QStringLiteral("GCJ-02"));
    QCOMPARE(result.coordinate.value(QStringLiteral("lat")).toDouble(), 31.2304);
    QCOMPARE(result.coordinate.value(QStringLiteral("lng")).toDouble(), 121.4737);
    QCOMPARE(result.formattedAddress, QStringLiteral("上海市人民广场"));

    const QByteArray limitedBody = QJsonDocument(QJsonObject{
        {QStringLiteral("status"), 120},
        {QStringLiteral("message"), QStringLiteral("调用量超过配额限制")}
    }).toJson(QJsonDocument::Compact);
    result = TencentMapAdapter::parseGeocodeResponse(
        limitedBody, QStringLiteral("人民广场"), QStringLiteral("上海市"));
    QCOMPARE(result.error, Charging::ErrorCode::RateLimited);

    const QByteArray emptyBody = QJsonDocument(QJsonObject{
        {QStringLiteral("status"), 347},
        {QStringLiteral("message"), QStringLiteral("无结果")}
    }).toJson(QJsonDocument::Compact);
    result = TencentMapAdapter::parseGeocodeResponse(
        emptyBody, QStringLiteral("不存在的地址"), QString());
    QCOMPARE(result.error, Charging::ErrorCode::NotFound);

    const QByteArray invalidCoordinateBody = QJsonDocument(QJsonObject{
        {QStringLiteral("status"), 0},
        {QStringLiteral("result"), QJsonObject{
            {QStringLiteral("location"), QJsonObject{
                {QStringLiteral("lat"), 100.0},
                {QStringLiteral("lng"), 121.0}
            }}
        }}
    }).toJson(QJsonDocument::Compact);
    result = TencentMapAdapter::parseGeocodeResponse(
        invalidCoordinateBody, QStringLiteral("异常地址"), QString());
    QCOMPARE(result.error, Charging::ErrorCode::NetworkUnavailable);
}

void Phase1Test::mapGeocodeRequiresAuthenticationAndValidInput()
{
    Charging::ClientConnection client;
    connectClient(client);
    Charging::Message response = request(
        client, Charging::MessageType::MapGeocodeRequest,
        Charging::MessageType::MapGeocodeResponse,
        {{QStringLiteral("address"), QStringLiteral("上海市人民广场")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Unauthorized);

    response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13300133000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);

    response = request(
        client, Charging::MessageType::MapGeocodeRequest,
        Charging::MessageType::MapGeocodeResponse,
        {{QStringLiteral("address"), QStringLiteral("   ")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::ValidationFailed);

    response = request(
        client, Charging::MessageType::MapGeocodeRequest,
        Charging::MessageType::MapGeocodeResponse,
        {{QStringLiteral("address"), 12345}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::InvalidPayload);
}

void Phase1Test::userCannotUseAdminCommand()
{
    Charging::ClientConnection client;
    connectClient(client);
    Charging::Message response = request(
        client, Charging::MessageType::UserLoginRequest,
        Charging::MessageType::UserLoginResponse,
        {{QStringLiteral("phone"), QStringLiteral("13900139000")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    response = request(client, Charging::MessageType::AdminCommandRequest,
                       Charging::MessageType::AdminCommandResponse);
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Forbidden);
}

void Phase1Test::adminLoginAndPasswordStorage()
{
    Charging::ClientConnection client;
    connectClient(client);
    Charging::Message response = request(
        client, Charging::MessageType::AdminLoginRequest,
        Charging::MessageType::AdminLoginResponse,
        {{QStringLiteral("username"), QStringLiteral("admin")},
         {QStringLiteral("password"), QStringLiteral("wrong-password")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::InvalidCredentials);

    response = request(
        client, Charging::MessageType::AdminLoginRequest,
        Charging::MessageType::AdminLoginResponse,
        {{QStringLiteral("username"), QStringLiteral("admin")},
         {QStringLiteral("password"), QStringLiteral("123456")}});
    QCOMPARE(response.header.statusCode, Charging::ErrorCode::Success);
    QCOMPARE(response.payload.value(QStringLiteral("role")).toString(),
             QStringLiteral("administrator"));

    const QString connectionName = QStringLiteral("phase1_verification");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          connectionName);
        database.setDatabaseName(m_databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "SELECT password_hash, password_salt FROM admins WHERE username='admin'")));
        QVERIFY(query.next());
        QVERIFY(query.value(0).toString() != QStringLiteral("123456"));
        QVERIFY(query.value(0).toString().size() >= 64);
        QVERIFY(query.value(1).toString().size() >= 32);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_GUILESS_MAIN(Phase1Test)
