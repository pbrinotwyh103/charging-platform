#include "phase1_test.h"

#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QElapsedTimer>
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
