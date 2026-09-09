#include "business_integration_test.h"
#include "protocol/packetcodec.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

using Charging::ErrorCode;
using Charging::Message;
using Charging::MessageType;
using Charging::ClientConnection;

namespace {
struct RequestPair { MessageType request; MessageType response; };
const QList<RequestPair> userPairs{
    {MessageType::UserProfileRequest, MessageType::UserProfileResponse},
    {MessageType::UserProfileUpdateRequest, MessageType::UserProfileUpdateResponse},
    {MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse},
    {MessageType::WalletLedgerRequest, MessageType::WalletLedgerResponse},
    {MessageType::StationListRequest, MessageType::StationListResponse},
    {MessageType::PileListRequest, MessageType::PileListResponse},
    {MessageType::FavoriteToggleRequest, MessageType::FavoriteToggleResponse},
    {MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse},
    {MessageType::ReservationCancelRequest, MessageType::ReservationCancelResponse},
    {MessageType::ChargingStartRequest, MessageType::ChargingStartResponse},
    {MessageType::ChargingStopRequest, MessageType::ChargingStopResponse},
    {MessageType::ActiveOrderRequest, MessageType::ActiveOrderResponse}
};

// A byte sink replaces only the operating-system TCP transport. The real
// dispatcher, session encoder, services, SQLite and event loop still execute.
class RecordingSocket final : public QTcpSocket {
public:
    RecordingSocket() { QIODevice::open(QIODevice::ReadWrite); setSocketState(ConnectedState); }
    QList<Message> messages;
    QList<QByteArray> writes;
    bool failWrites = false;
protected:
    qint64 writeData(const char *data, qint64 size) override {
        if (failWrites) return -1;
        QByteArray buffer(data, size);
        writes.append(buffer);
        const auto decoded = Charging::PacketCodec::tryDecode(buffer);
        if (decoded.status == Charging::DecodeStatus::Complete) messages.append(decoded.message);
        return size;
    }
};

Message packet(MessageType type, quint32 id, const QJsonObject &payload = {})
{
    Message result;
    result.header.messageType = type;
    result.header.requestId = id;
    result.payload = payload;
    return result;
}

bool takeRequestSlot(ClientSession &session, quint32 id)
{
    if (!session.markRequestStarted(id)) return false;
    session.finishRequest(id);
    return true;
}

QJsonObject paddedAdminPayload(qsizetype targetBytes)
{
    QJsonObject payload{{"action", "dashboard.summary"}};
    const auto originalBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact).size();
    payload.insert("action", payload.value("action").toString() + QString(targetBytes - originalBytes, ' '));
    return payload;
}

bool chargingFrameFor(const Message &message, const QJsonValue &orderId)
{
    return message.header.requestId == 0 && message.payload.value("orderId") == orderId
        && (message.header.messageType == MessageType::ChargingProgressPush
            || message.header.messageType == MessageType::ChargingStoppedPush);
}
}

void BusinessIntegrationTest::init()
{
    m_directory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_directory->isValid());
    m_databasePath = m_directory->filePath("business.db");
    if (QByteArray(QTest::currentTestFunction()).startsWith("direct")) return;
    m_server = std::make_unique<ServerApplication>();
    QString error;
    QVERIFY2(m_server->start(0, m_databasePath, &error), qPrintable(error));
}

void BusinessIntegrationTest::cleanup()
{
    m_server.reset();
    m_directory.reset();
}

void BusinessIntegrationTest::connectClient(ClientConnection &client)
{
    client.setAutoReconnect(false);
    client.connectToServer("127.0.0.1", m_server->listeningPort());
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 3000);
}

Message BusinessIntegrationTest::take(QSignalSpy &spy, MessageType type,
                                      quint32 requestId, int timeout,
                                      const std::function<bool(const Message &)> &matches)
{
    return takeMatching(spy, [&](const Message &message) {
        return message.header.requestId == requestId && message.header.messageType == type
            && (!matches || matches(message));
    }, timeout);
}

Message BusinessIntegrationTest::takeMatching(QSignalSpy &spy,
    const std::function<bool(const Message &)> &matches, int timeout)
{
    QElapsedTimer timer;
    timer.start();
    do {
        for (qsizetype i = 0; i < spy.size(); ++i) {
            const auto message = qvariant_cast<Message>(spy.at(i).at(0));
            if (matches(message)) {
                spy.removeAt(i);
                return message;
            }
        }
        if (timer.elapsed() < timeout) spy.wait(timeout - int(timer.elapsed()));
    } while (timer.elapsed() < timeout);
    QTest::qFail("Missing expected server message before deadline", __FILE__, __LINE__);
    return {};
}

Message BusinessIntegrationTest::takeChargingFrame(QSignalSpy &spy, const QJsonValue &orderId,
                                                   int timeout)
{
    // Filter only the order and event family. The next matching arrival must
    // be checked, even when its seq/type differs from the expected frame.
    return takeMatching(spy, [&](const Message &message) {
        return chargingFrameFor(message, orderId);
    }, timeout);
}

QString BusinessIntegrationTest::compareNextPeerFrame(QSignalSpy &peer, const Message &owner,
                                                       qint64 *previousSequence, int timeout)
{
    const auto received = takeChargingFrame(peer, owner.payload.value("orderId"), timeout);
    const auto sequence = received.payload.value("seq").toInteger();
    if (sequence != *previousSequence + 1) return "Peer sequence was not consecutive";
    *previousSequence = sequence;
    if (received.header.messageType != owner.header.messageType || received.payload != owner.payload)
        return "Next peer frame differs from corresponding owner frame";
    return {};
}

Message BusinessIntegrationTest::request(ClientConnection &client, MessageType type,
    MessageType response, const QJsonObject &payload, ErrorCode status, quint32 requestId)
{
    QSignalSpy spy(&client, &ClientConnection::messageReceived);
    const auto id = requestId ? requestId : client.nextRequestId();
    if (!client.send(type, id, payload)) {
        QTest::qFail("Could not send request", __FILE__, __LINE__);
        return {};
    }
    // Match only the request ID here so wrong response types fail immediately.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        for (qsizetype i = 0; i < spy.size(); ++i) {
            const auto message = qvariant_cast<Message>(spy.at(i).at(0));
            if (message.header.requestId != id) continue;
            QTest::qCompare(message.header.messageType, response, "response type", "expected", __FILE__, __LINE__);
            QTest::qCompare(message.header.statusCode, status, "status", "expected", __FILE__, __LINE__);
            if (message.payload.value("message").toString().trimmed().isEmpty())
                QTest::qFail("Every response needs a display message", __FILE__, __LINE__);
            return message;
        }
        spy.clear();
        spy.wait(3000 - int(timer.elapsed()));
    }
    QTest::qFail("Response request ID was not preserved", __FILE__, __LINE__);
    return {};
}

Message BusinessIntegrationTest::login(ClientConnection &client, const QString &phone)
{
    connectClient(client);
    return request(client, MessageType::UserLoginRequest, MessageType::UserLoginResponse, {{"phone", phone}});
}

Message BusinessIntegrationTest::loginAdmin(ClientConnection &client)
{
    connectClient(client);
    return request(client, MessageType::AdminLoginRequest, MessageType::AdminLoginResponse,
                   {{"username", "admin"}, {"password", "123456"}});
}

QVariant BusinessIntegrationTest::sql(const QString &statement)
{
    QVariant value;
    const QString name = "business_fixture";
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(m_databasePath);
        if (!db.open()) QTest::qFail(qPrintable(db.lastError().text()), __FILE__, __LINE__);
        QSqlQuery query(db);
        if (!query.exec(statement)) QTest::qFail(qPrintable(query.lastError().text()), __FILE__, __LINE__);
        if (query.next()) value = query.value(0);
    }
    QSqlDatabase::removeDatabase(name);
    return value;
}

void BusinessIntegrationTest::triggerJob(const char *signal)
{
    auto *jobs = m_server->findChild<JobManager *>();
    QVERIFY(jobs);
    QVERIFY(QMetaObject::invokeMethod(jobs, signal, Qt::DirectConnection));
}

void BusinessIntegrationTest::unauthorizedResponses_data()
{
    QTest::addColumn<int>("requestType");
    QTest::addColumn<int>("responseType");
    auto pairs = userPairs;
    pairs.append({MessageType::AdminCommandRequest, MessageType::AdminCommandResponse});
    pairs.append({MessageType::LogoutRequest, MessageType::LogoutResponse});
    for (const auto &pair : pairs)
        QTest::newRow(qPrintable(QString::number(int(pair.request)))) << int(pair.request) << int(pair.response);
}

void BusinessIntegrationTest::unauthorizedResponses()
{
    QFETCH(int, requestType);
    QFETCH(int, responseType);
    ClientConnection client;
    connectClient(client);
    request(client, MessageType(requestType), MessageType(responseType), {}, ErrorCode::Unauthorized, 918273);
    QSignalSpy spy(&client, &ClientConnection::messageReceived);
    QVERIFY(client.send(MessageType(requestType), 0));
    const auto invalid = take(spy, MessageType(responseType), 0);
    QCOMPARE(invalid.header.statusCode, ErrorCode::InvalidPacket);
    QVERIFY(!invalid.payload.value("message").toString().isEmpty());
}

void BusinessIntegrationTest::roleGuards()
{
    ClientConnection admin, user;
    loginAdmin(admin);
    login(user);
    for (const auto &pair : userPairs)
        request(admin, pair.request, pair.response, {}, ErrorCode::Forbidden);
    request(user, MessageType::AdminCommandRequest, MessageType::AdminCommandResponse,
            {{"action", "dashboard.summary"}}, ErrorCode::Forbidden);
    request(user, MessageType::AdminLoginRequest, MessageType::AdminLoginResponse, {}, ErrorCode::Forbidden);
    request(admin, MessageType::UserLoginRequest, MessageType::UserLoginResponse, {}, ErrorCode::Forbidden);
    request(user, MessageType(65000), MessageType(65000), {}, ErrorCode::UnsupportedMessage);
}

void BusinessIntegrationTest::invalidFields_data()
{
    QTest::addColumn<int>("requestType");
    QTest::addColumn<int>("responseType");
    QTest::addColumn<QJsonObject>("payload");
    QTest::addColumn<int>("status");
    auto row = [](const char *name, MessageType req, MessageType res, QJsonObject payload,
                  ErrorCode status = ErrorCode::ValidationFailed) {
        QTest::newRow(name) << int(req) << int(res) << payload << int(status);
    };
    row("profile", MessageType::UserProfileUpdateRequest, MessageType::UserProfileUpdateResponse, {{"nickname", 123}});
    row("recharge", MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse, {{"amountCents", "100"}, {"transactionId", "bad"}});
    row("ledger", MessageType::WalletLedgerRequest, MessageType::WalletLedgerResponse, {{"page", 0}});
    row("stations", MessageType::StationListRequest, MessageType::StationListResponse, {{"radiusKm", 101}});
    row("piles", MessageType::PileListRequest, MessageType::PileListResponse, {{"stationId", "1"}});
    row("favorite", MessageType::FavoriteToggleRequest, MessageType::FavoriteToggleResponse, {{"stationId", 1}, {"favorited", 1}});
    row("reservation", MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse, {{"stationId", 1}, {"durationMinutes", 0}});
    row("cancel", MessageType::ReservationCancelRequest, MessageType::ReservationCancelResponse, {{"reservationId", 0}});
    row("start", MessageType::ChargingStartRequest, MessageType::ChargingStartResponse, {{"reservationId", "1"}});
    row("stop", MessageType::ChargingStopRequest, MessageType::ChargingStopResponse, {{"orderId", 0}});
    row("admin-fields", MessageType::AdminCommandRequest, MessageType::AdminCommandResponse, {{"action", 1}});
    row("admin-unknown", MessageType::AdminCommandRequest, MessageType::AdminCommandResponse, {{"action", "unknown.action"}}, ErrorCode::UnsupportedMessage);
}

void BusinessIntegrationTest::invalidFields()
{
    QFETCH(int, requestType);
    QFETCH(int, responseType);
    QFETCH(QJsonObject, payload);
    QFETCH(int, status);
    ClientConnection client;
    if (MessageType(requestType) == MessageType::AdminCommandRequest) loginAdmin(client);
    else login(client);
    request(client, MessageType(requestType), MessageType(responseType), payload, ErrorCode(status));
}

void BusinessIntegrationTest::userWorkflowAndPushes()
{
    ClientConnection user, secondSession, otherUser, admin, anonymous;
    const auto userId = login(user).payload.value("userId").toInteger();
    login(secondSession);
    login(otherUser, "13800138802");
    loginAdmin(admin);
    connectClient(anonymous);
    auto *tcp = m_server->findChild<TcpServer *>();
    QVERIFY(tcp);
    QCOMPARE(tcp->sessionsForUser(userId).size(), 2);
    QCOMPARE(tcp->administratorSessions().size(), 1);

    request(user, MessageType::UserProfileUpdateRequest, MessageType::UserProfileUpdateResponse, {{"nickname", "集成用户"}});
    QCOMPARE(request(user, MessageType::UserProfileRequest, MessageType::UserProfileResponse).payload.value("nickname").toString(), QString("集成用户"));
    const auto recharge = request(user, MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse,
                                  {{"amountCents", 10000}, {"transactionId", "integration-recharge"}});
    QCOMPARE(recharge.payload.value("balanceCents").toInt(), 10000);
    QCOMPARE(request(user, MessageType::WalletLedgerRequest, MessageType::WalletLedgerResponse).payload.value("total").toInt(), 1);
    QVERIFY(!request(user, MessageType::StationListRequest, MessageType::StationListResponse).payload.value("items").toArray().isEmpty());
    QCOMPARE(request(user, MessageType::PileListRequest, MessageType::PileListResponse, {{"stationId", 1}}).payload.value("items").toArray().size(), 2);
    QVERIFY(request(user, MessageType::FavoriteToggleRequest, MessageType::FavoriteToggleResponse, {{"stationId", 1}, {"favorited", true}}).payload.value("favorited").toBool());
    // The profile page uses the same station-list protocol with favoritesOnly
    // to render the user's real collection. Verify this path over the full
    // TCP dispatcher/session stack, including the current-user scope.
    const auto favoriteStations = request(user, MessageType::StationListRequest,
                                          MessageType::StationListResponse,
                                          {{"favoritesOnly", true}, {"page", 1},
                                           {"pageSize", 50}, {"sort", "name"}}).payload;
    QCOMPARE(favoriteStations.value("total").toInt(), 1);
    const auto favoriteItems = favoriteStations.value("items").toArray();
    QCOMPARE(favoriteItems.size(), 1);
    QCOMPARE(favoriteItems.first().toObject().value("stationId").toInteger(), qint64(1));
    QVERIFY(favoriteItems.first().toObject().value("favorited").toBool());
    QVERIFY(!request(user, MessageType::ActiveOrderRequest, MessageType::ActiveOrderResponse).payload.value("active").toBool());
    auto reservation = request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse,
                               {{"stationId", 1}, {"pileId", 1}}).payload;
    QCOMPARE(request(user, MessageType::ReservationCancelRequest, MessageType::ReservationCancelResponse,
                     {{"reservationId", reservation.value("reservationId")}}).payload.value("status").toString(), QString("cancelled"));
    reservation = request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse,
                           {{"stationId", 1}, {"pileId", 1}}).payload;
    QSignalSpy userPush(&user, &ClientConnection::messageReceived), secondPush(&secondSession, &ClientConnection::messageReceived),
        adminPush(&admin, &ClientConnection::messageReceived);
    const auto started = request(user, MessageType::ChargingStartRequest, MessageType::ChargingStartResponse,
                                 {{"reservationId", reservation.value("reservationId")}}).payload;
    const auto orderId = started.value("orderId");
    QCOMPARE(request(user, MessageType::ActiveOrderRequest, MessageType::ActiveOrderResponse).payload.value("orderId"), orderId);
    request(otherUser, MessageType::ChargingStopRequest, MessageType::ChargingStopResponse,
            {{"orderId", orderId}}, ErrorCode::Forbidden);
    QSignalSpy otherPush(&otherUser, &ClientConnection::messageReceived),
        anonymousPush(&anonymous, &ClientConnection::messageReceived);
    // The server's global timer can first sample a newly started order at age
    // zero. Check every stream in arrival order while waiting for elapsed time.
    Message progress;
    qint64 previousSequence = started.value("seq").toInteger();
    qint64 adminSequence = previousSequence;
    qint64 secondSequence = previousSequence;
    QString peerError;
    QElapsedTimer progressDeadline;
    progressDeadline.start();
    do {
        const int remaining = 3000 - int(progressDeadline.elapsed());
        QVERIFY2(remaining > 0, "Charging progress did not reach one elapsed second");
        progress = takeChargingFrame(userPush, orderId, remaining);
        QCOMPARE(progress.header.messageType, MessageType::ChargingProgressPush);
        const auto sequence = progress.payload.value("seq").toInteger();
        QCOMPARE(sequence, previousSequence + 1);
        previousSequence = sequence;
        peerError = compareNextPeerFrame(adminPush, progress, &adminSequence,
                                         qMax(1, 3000 - int(progressDeadline.elapsed())));
        QVERIFY2(peerError.isEmpty(), qPrintable(peerError));
        peerError = compareNextPeerFrame(secondPush, progress, &secondSequence,
                                         qMax(1, 3000 - int(progressDeadline.elapsed())));
        QVERIFY2(peerError.isEmpty(), qPrintable(peerError));
    } while (progress.payload.value("durationSec").toInt() < 1);
    QCOMPARE(progress.payload.value("orderId"), orderId);
    QVERIFY(progress.payload.value("seq").toInt() > 0);
    QVERIFY(progress.payload.value("durationSec").toInt() >= 1);
    // MonitorPage in origin/admin-client reads this long-form field.
    QCOMPARE(progress.payload.value("durationSeconds"), progress.payload.value("durationSec"));
    QVERIFY(progress.payload.value("powerKw").toDouble() > 0);
    const auto stopped = request(user, MessageType::ChargingStopRequest, MessageType::ChargingStopResponse, {{"orderId", orderId}}).payload;
    QCOMPARE(stopped.value("status").toString(), QString("completed"));
    QCOMPARE(stopped.value("stopReason").toString(), QString("user_stop"));
    QCOMPARE(stopped.value("balanceCents").toInt() + stopped.value("feeCents").toInt(), 10000);
    // A progress tick may already be queued when stop is requested. Consume
    // those frames on all three streams before accepting the stopped frame.
    Message final;
    QElapsedTimer stopDeadline;
    stopDeadline.start();
    do {
        const int remaining = 3000 - int(stopDeadline.elapsed());
        QVERIFY2(remaining > 0, "Missing final charging event");
        final = takeChargingFrame(userPush, orderId, remaining);
        const auto sequence = final.payload.value("seq").toInteger();
        QCOMPARE(sequence, previousSequence + 1);
        previousSequence = sequence;
        peerError = compareNextPeerFrame(adminPush, final, &adminSequence,
                                         qMax(1, 3000 - int(stopDeadline.elapsed())));
        QVERIFY2(peerError.isEmpty(), qPrintable(peerError));
        peerError = compareNextPeerFrame(secondPush, final, &secondSequence,
                                         qMax(1, 3000 - int(stopDeadline.elapsed())));
        QVERIFY2(peerError.isEmpty(), qPrintable(peerError));
    } while (final.header.messageType != MessageType::ChargingStoppedPush);
    QCOMPARE(final.payload.value("feeCents"), stopped.value("feeCents"));
    QVERIFY(final.payload.value("seq").toInt() > progress.payload.value("seq").toInt());
    QVERIFY(otherPush.isEmpty());
    QVERIFY(anonymousPush.isEmpty());
    QVERIFY(!request(user, MessageType::ActiveOrderRequest, MessageType::ActiveOrderResponse).payload.value("active").toBool());
    for (auto *stream : {&userPush, &adminPush, &secondPush}) {
        for (const auto &arguments : *stream)
            QVERIFY2(!chargingFrameFor(qvariant_cast<Message>(arguments.at(0)), orderId),
                     "Unchecked charging frame remained after the stopped event");
    }
    request(secondSession, MessageType::LogoutRequest, MessageType::LogoutResponse);
    QCOMPARE(tcp->sessionsForUser(userId).size(), 1);
    user.disconnectFromServer();
    QTRY_VERIFY(tcp->sessionsForUser(userId).isEmpty());
    admin.disconnectFromServer();
    QTRY_VERIFY(tcp->administratorSessions().isEmpty());
}

void BusinessIntegrationTest::faultsReachOwnerAndAdministrators()
{
    ClientConnection user, admin, stranger;
    login(user);
    loginAdmin(admin);
    login(stranger, "13800138802");
    const auto denied = request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse,
                                {{"stationId", 1}}, ErrorCode::Conflict);
    QCOMPARE(denied.payload.value("reason").toString(), QString("insufficient_balance"));
    request(user, MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse,
            {{"amountCents", 10000}, {"transactionId", "fault-recharge"}});
    const auto reservation = request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse,
                                     {{"stationId", 1}, {"pileId", 1}}).payload;
    const auto orderId = request(user, MessageType::ChargingStartRequest, MessageType::ChargingStartResponse,
                                 {{"reservationId", reservation.value("reservationId")}}).payload.value("orderId");
    QSignalSpy userPush(&user, &ClientConnection::messageReceived), adminPush(&admin, &ClientConnection::messageReceived),
        strangerPush(&stranger, &ClientConnection::messageReceived);
    sql("UPDATE charging_piles SET status='fault' WHERE id=1");
    triggerJob("chargingTick");
    const auto alarm = take(userPush, MessageType::AlarmPush, 0);
    QCOMPARE(alarm.payload.value("orderId"), orderId);
    QCOMPARE(alarm.payload.value("alarmType").toString(), QString("device_fault"));
    QCOMPARE(take(adminPush, MessageType::AlarmPush, 0).payload, alarm.payload);
    const auto stopped = take(userPush, MessageType::ChargingStoppedPush, 0);
    QCOMPARE(stopped.payload.value("status").toString(), QString("fault_stopped"));
    QCOMPARE(take(adminPush, MessageType::ChargingStoppedPush, 0).payload, stopped.payload);
    QVERIFY(strangerPush.isEmpty());
    triggerJob("chargingTick");
    QTest::qWait(100);
    QVERIFY(userPush.isEmpty());
    QCOMPARE(sql("SELECT COUNT(*) FROM alarms").toInt(), 1);
}

void BusinessIntegrationTest::reservationExpiryJob()
{
    ClientConnection user;
    login(user);
    request(user, MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse,
            {{"amountCents", 10000}, {"transactionId", "expiry-recharge"}});
    request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse, {{"stationId", 1}, {"pileId", 1}});
    sql("UPDATE reservations SET expires_at='2000-01-01T00:00:00Z' WHERE status='active'");
    triggerJob("reservationExpiryTick");
    QTRY_COMPARE_WITH_TIMEOUT(sql("SELECT status FROM reservations").toString(), QString("expired"), 3000);
    QCOMPARE(sql("SELECT status FROM charging_piles WHERE id=1").toString(), QString("idle"));
}

void BusinessIntegrationTest::duplicateRequestLifecycle()
{
    ClientConnection user;
    login(user);
    QSignalSpy spy(&user, &ClientConnection::messageReceived);
    const QJsonObject payload{{"amountCents", 1000}, {"transactionId", "duplicate"}};
    QVERIFY(user.send(MessageType::WalletRechargeRequest, 4545, payload));
    QVERIFY(user.send(MessageType::WalletRechargeRequest, 4545, payload));
    const auto first = take(spy, MessageType::WalletRechargeResponse, 4545);
    const auto second = take(spy, MessageType::WalletRechargeResponse, 4545);
    QSet<int> statuses{int(first.header.statusCode), int(second.header.statusCode)};
    QCOMPARE(statuses, QSet<int>({int(ErrorCode::Success), int(ErrorCode::DuplicateRequest)}));
    const auto ledger = request(user, MessageType::WalletLedgerRequest, MessageType::WalletLedgerResponse,
                                {}, ErrorCode::Success, 4545);
    QCOMPARE(ledger.payload.value("total").toInt(), 1);
    QCOMPARE(sql("SELECT balance_cents FROM users").toInt(), 1000);
}

void BusinessIntegrationTest::rechargeCannotBlockOrderSettlement()
{
    ClientConnection user;
    login(user);
    const QJsonObject payload{{"amountCents", 1000}, {"transactionId", "ORDER-PAYMENT-1"}};
    const auto recharge = request(user, MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse, payload);
    QCOMPARE(recharge.payload.value("transactionId").toString(), QString("ORDER-PAYMENT-1"));
    const auto reservation = request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse,
                                     {{"stationId", 1}, {"pileId", 1}});
    const auto started = request(user, MessageType::ChargingStartRequest, MessageType::ChargingStartResponse,
                                 {{"reservationId", reservation.payload.value("reservationId")}});
    QCOMPARE(started.payload.value("orderId").toInt(), 1);
    const auto stopped = request(user, MessageType::ChargingStopRequest, MessageType::ChargingStopResponse, {{"orderId", 1}});
    QCOMPARE(stopped.payload.value("status").toString(), QString("completed"));
    QCOMPARE(request(user, MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse, payload).payload, recharge.payload);
    QCOMPARE(request(user, MessageType::ChargingStopRequest, MessageType::ChargingStopResponse, {{"orderId", 1}}).payload, stopped.payload);
    QCOMPARE(sql("SELECT COUNT(*) FROM wallet_records").toInt(), 2);
    QCOMPARE(sql("SELECT status FROM charging_piles WHERE id=1").toString(), QString("idle"));
    QCOMPARE(sql("SELECT balance_cents FROM users WHERE id=1").toInt(), stopped.payload.value("balanceCents").toInt());
}

void BusinessIntegrationTest::adminScopeComesFromSession()
{
    ClientConnection first, second;
    const auto firstScope = loginAdmin(first).payload.value("sessionId").toString();
    const auto secondScope = loginAdmin(second).payload.value("sessionId").toString();
    QVERIFY(firstScope != secondScope);
    request(first, MessageType::AdminCommandRequest, MessageType::AdminCommandResponse,
            {{"action", "pile.disable"}, {"pileId", 1}, {"_requestScope", "forged"}}, ErrorCode::Success, 777);
    request(first, MessageType::AdminCommandRequest, MessageType::AdminCommandResponse,
            {{"action", "pile.enable"}, {"pileId", 1}}, ErrorCode::Success, 778);
    request(second, MessageType::AdminCommandRequest, MessageType::AdminCommandResponse,
            {{"action", "pile.disable"}, {"pileId", 1}, {"_requestScope", "forged"}}, ErrorCode::Success, 777);
    QCOMPARE(sql("SELECT status FROM charging_piles WHERE id=1").toString(), QString("disabled"));
    const auto firstDetail = QJsonDocument::fromJson(sql("SELECT detail FROM device_control_records WHERE request_id=777 ORDER BY id LIMIT 1").toByteArray()).object();
    const auto secondDetail = QJsonDocument::fromJson(sql("SELECT detail FROM device_control_records WHERE request_id=777 ORDER BY id DESC LIMIT 1").toByteArray()).object();
    QCOMPARE(firstDetail.value("request").toObject().value("_requestScope").toString(), firstScope);
    QCOMPARE(secondDetail.value("request").toObject().value("_requestScope").toString(), secondScope);
}

void BusinessIntegrationTest::logoutDiscardsPendingProfile()
{
    ClientConnection user;
    login(user);
    QSignalSpy spy(&user, &ClientConnection::messageReceived);
    QVERIFY(user.send(MessageType::UserProfileRequest, 880));
    QVERIFY(user.send(MessageType::LogoutRequest, 881));
    QCOMPARE(take(spy, MessageType::LogoutResponse, 881).header.statusCode, ErrorCode::Success);
    const auto profile = take(spy, MessageType::UserProfileResponse, 880);
    QCOMPARE(profile.header.statusCode, ErrorCode::SessionExpired);
    QVERIFY(!profile.payload.contains("phone"));
}

void BusinessIntegrationTest::directRequestMappingsAndLifecycle()
{
    DatabaseManager database;
    ServiceRegistry services;
    QString error;
    QVERIFY2(database.open(m_databasePath, &error), qPrintable(error));
    QVERIFY2(services.initialize(&database, &error), qPrintable(error));
    MessageDispatcher dispatcher(&services);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    quint32 id = 1;
    auto pairs = userPairs;
    pairs.append({MessageType::AdminCommandRequest, MessageType::AdminCommandResponse});
    pairs.append({MessageType::LogoutRequest, MessageType::LogoutResponse});
    for (const auto &pair : pairs) {
        dispatcher.dispatch(&session, packet(pair.request, id));
        QCOMPARE(socket->messages.last().header.messageType, pair.response);
        QCOMPARE(socket->messages.last().header.requestId, id);
        QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::Unauthorized);
        QVERIFY(!socket->messages.last().payload.value("message").toString().isEmpty());
        QVERIFY(takeRequestSlot(session, id++));
    }
    socket->messages.clear();
    dispatcher.dispatch(&session, packet(MessageType::UserLoginRequest, 40, {{"phone", "13800138801"}}));
    QVERIFY(!session.markRequestStarted(40));
    QTRY_COMPARE(socket->messages.size(), 1);
    QCOMPARE(socket->messages.first().header.statusCode, ErrorCode::Success);
    QCOMPARE(socket->messages.first().payload.value("avatar").toString(), QString("default://gray-avatar"));
    QVERIFY(session.isAuthenticated());
    QVERIFY(takeRequestSlot(session, 40));
    socket->messages.clear();
    const auto recharge = packet(MessageType::WalletRechargeRequest, 41,
                                 {{"amountCents", 1000}, {"transactionId", "direct-recharge"}});
    dispatcher.dispatch(&session, recharge);
    dispatcher.dispatch(&session, recharge);
    QCOMPARE(socket->messages.size(), 1);
    QCOMPARE(socket->messages.first().header.statusCode, ErrorCode::DuplicateRequest);
    QVERIFY(!session.markRequestStarted(41));
    QTRY_COMPARE(socket->messages.size(), 2);
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::Success);
    QVERIFY(takeRequestSlot(session, 41));
    QCOMPARE(sql("SELECT balance_cents FROM users").toInt(), 1000);

    socket->messages.clear();
    dispatcher.dispatch(&session, packet(MessageType::UserProfileRequest, 42));
    dispatcher.dispatch(&session, packet(MessageType::LogoutRequest, 43));
    QTRY_COMPARE(socket->messages.size(), 2);
    QCOMPARE(socket->messages.last().header.messageType, MessageType::UserProfileResponse);
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::SessionExpired);
    QVERIFY(!socket->messages.last().payload.contains("phone"));
    QVERIFY(takeRequestSlot(session, 42));
    QVERIFY(!session.isAuthenticated());
}

void BusinessIntegrationTest::directTimeoutKeepsRequestReserved()
{
    DatabaseManager database;
    ServiceRegistry services;
    QString error;
    QVERIFY2(database.open(m_databasePath, &error), qPrintable(error));
    QVERIFY2(services.initialize(&database, &error), qPrintable(error));
    MessageDispatcher dispatcher(&services, nullptr, 50);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    // A real SQLite writer lock delays auto-registration without replacing the
    // service under test. The event loop remains free to deliver the deadline.
    QSqlQuery lock(database.database());
    QVERIFY(lock.exec("BEGIN IMMEDIATE"));
    const auto loginPacket = packet(MessageType::UserLoginRequest, 50, {{"phone", "13800138801"}});
    dispatcher.dispatch(&session, loginPacket);
    QTRY_COMPARE_WITH_TIMEOUT(socket->messages.size(), 1, 1000);
    QCOMPARE(socket->messages.first().header.statusCode, ErrorCode::RequestTimeout);
    QCOMPARE(socket->messages.first().header.messageType, MessageType::UserLoginResponse);
    QCOMPARE(socket->messages.first().header.requestId, quint32(50));
    dispatcher.dispatch(&session, loginPacket);
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::DuplicateRequest);
    QVERIFY(lock.exec("ROLLBACK"));
    QTRY_VERIFY_WITH_TIMEOUT(takeRequestSlot(session, 50), 3000);
    QCOMPARE(socket->messages.size(), 2); // No second response after timeout.
    QVERIFY(!session.isAuthenticated()); // A late login cannot silently sign in.
}

void BusinessIntegrationTest::directAdminScopeAndDisconnect()
{
    DatabaseManager database;
    ServiceRegistry services;
    QString error;
    QVERIFY2(database.open(m_databasePath, &error), qPrintable(error));
    QVERIFY2(services.initialize(&database, &error), qPrintable(error));
    MessageDispatcher dispatcher(&services);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    session.authenticate(Charging::Role::Administrator, 1, "admin");
    const auto scope = session.sessionId();
    dispatcher.dispatch(&session, packet(MessageType::AdminCommandRequest, 70,
        {{"action", "pile.disable"}, {"pileId", 1}, {"_requestScope", "forged"}}));
    QTRY_COMPARE(socket->messages.size(), 1);
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::Success);
    const auto detail = QJsonDocument::fromJson(sql("SELECT detail FROM device_control_records").toByteArray()).object();
    QCOMPARE(detail.value("request").toObject().value("_requestScope").toString(), scope);
    for (const auto &pair : userPairs) {
        dispatcher.dispatch(&session, packet(pair.request, 71));
        QCOMPARE(socket->messages.last().header.messageType, pair.response);
        QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::Forbidden);
        QVERIFY(takeRequestSlot(session, 71));
    }
    {
        auto disconnected = std::make_unique<ClientSession>(new RecordingSocket);
        disconnected->authenticate(Charging::Role::Administrator, 1, "admin");
        dispatcher.dispatch(disconnected.get(), packet(MessageType::AdminCommandRequest, 72,
            {{"action", "pile.enable"}, {"pileId", 1}}));
        disconnected.reset();
    }
    QTRY_COMPARE_WITH_TIMEOUT(sql("SELECT status FROM charging_piles WHERE id=1").toString(), QString("idle"), 3000);
    // Dispatcher teardown also releases a living session's in-flight ID.
    auto shortLived = std::make_unique<MessageDispatcher>(&services);
    shortLived->dispatch(&session, packet(MessageType::AdminCommandRequest, 73,
        {{"action", "dashboard.summary"}}));
    shortLived.reset();
    QVERIFY(takeRequestSlot(session, 73));
}

void BusinessIntegrationTest::directSessionReportsQueueFailure()
{
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    QVERIFY(session.send(MessageType::ChargingProgressPush, 0, {{"orderId", 1}}));
    socket->failWrites = true;
    QVERIFY(!session.send(MessageType::ChargingProgressPush, 0, {{"orderId", 1}}));
    QCOMPARE(socket->messages.size(), 1);
    ClientSession disconnected(new QTcpSocket);
    QVERIFY(!disconnected.send(MessageType::ChargingStoppedPush, 0));
}

void BusinessIntegrationTest::directLoginCannotSurviveAuthenticationAba()
{
    DatabaseManager database;
    ServiceRegistry services;
    QString error;
    QVERIFY2(database.open(m_databasePath, &error), qPrintable(error));
    QVERIFY2(services.initialize(&database, &error), qPrintable(error));
    const auto existing = services.auth()->loginUser("13800138802");
    QVERIFY(existing.succeeded());
    MessageDispatcher dispatcher(&services);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    // Login A must insert a new user and waits for the writer lock; login B
    // reads an existing user and can finish while A is still pending.
    QSqlQuery lock(database.database());
    QVERIFY(lock.exec("BEGIN IMMEDIATE"));
    dispatcher.dispatch(&session, packet(MessageType::UserLoginRequest, 101,
                                        {{"phone", "13800138801"}}));
    dispatcher.dispatch(&session, packet(MessageType::UserLoginRequest, 102,
                                        {{"phone", "13800138802"}}));
    QTRY_COMPARE_WITH_TIMEOUT(socket->messages.size(), 1, 3000);
    QCOMPARE(socket->messages.first().header.requestId, quint32(102));
    QCOMPARE(socket->messages.first().header.statusCode, ErrorCode::Success);
    QCOMPARE(session.principalId(), existing.principalId);
    dispatcher.dispatch(&session, packet(MessageType::LogoutRequest, 103));
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::Success);
    QVERIFY(session.sessionId().isEmpty());
    QVERIFY(lock.exec("ROLLBACK"));
    QTRY_COMPARE_WITH_TIMEOUT(socket->messages.size(), 3, 3000);
    QCOMPARE(socket->messages.last().header.requestId, quint32(101));
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::SessionExpired);
    QVERIFY(!socket->messages.last().payload.contains("phone"));
    QVERIFY(!session.isAuthenticated());
    QVERIFY(takeRequestSlot(session, 101));
}

void BusinessIntegrationTest::directAnonymousClearInvalidatesLogin_data()
{
    QTest::addColumn<bool>("logoutRequest");
    QTest::addColumn<bool>("deadline");
    QTest::newRow("clear-completion") << false << false;
    QTest::newRow("logout-completion") << true << false;
    QTest::newRow("clear-deadline") << false << true;
    QTest::newRow("logout-deadline") << true << true;
}

void BusinessIntegrationTest::directAnonymousClearInvalidatesLogin()
{
    QFETCH(bool, logoutRequest);
    QFETCH(bool, deadline);
    DatabaseManager database;
    ServiceRegistry services;
    QString error;
    QVERIFY2(database.open(m_databasePath, &error), qPrintable(error));
    QVERIFY2(services.initialize(&database, &error), qPrintable(error));
    MessageDispatcher dispatcher(&services, nullptr, deadline ? 50 : 10000);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    QSqlQuery lock(database.database());
    QVERIFY(lock.exec("BEGIN IMMEDIATE"));
    dispatcher.dispatch(&session, packet(MessageType::UserLoginRequest, 104,
                                        {{"phone", "13800138801"}}));
    if (logoutRequest) {
        dispatcher.dispatch(&session, packet(MessageType::LogoutRequest, 105));
        QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::Unauthorized);
        socket->messages.clear();
    } else {
        session.clearAuthentication();
    }
    QVERIFY(session.sessionId().isEmpty());
    if (!deadline) QVERIFY(lock.exec("ROLLBACK"));
    QTRY_COMPARE_WITH_TIMEOUT(socket->messages.size(), 1, 3000);
    if (deadline) QVERIFY(lock.exec("ROLLBACK"));
    QCOMPARE(socket->messages.last().header.statusCode, ErrorCode::SessionExpired);
    QCOMPARE(socket->messages.last().header.requestId, quint32(104));
    QTRY_VERIFY_WITH_TIMEOUT(takeRequestSlot(session, 104), 3000);
    QCOMPARE(socket->messages.size(), 1);
    QVERIFY(!session.isAuthenticated());
}

void BusinessIntegrationTest::directOutboundEnvelopeBoundaries_data()
{
    QTest::addColumn<int>("delta");
    QTest::addColumn<int>("status");
    QTest::newRow("below-limit") << -1 << int(ErrorCode::Success);
    QTest::newRow("at-limit") << 0 << int(ErrorCode::Success);
    QTest::newRow("oversized-success") << 1 << int(ErrorCode::Success);
    QTest::newRow("oversized-error") << 1 << int(ErrorCode::ValidationFailed);
}

void BusinessIntegrationTest::directOutboundEnvelopeBoundaries()
{
    QFETCH(int, delta);
    QFETCH(int, status);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    // The literal JSON {"message":""} contributes exactly 14 UTF-8 bytes.
    const QJsonObject payload{{"message", QString(Charging::MessageHeader::MaxPayloadLength - 14 + delta, 'x')}};
    QVERIFY(session.send(MessageType::AdminCommandResponse, 201, payload, ErrorCode(status)));
    QCOMPARE(socket->writes.size(), 1);
    auto bytes = socket->writes.first();
    const auto decoded = Charging::PacketCodec::tryDecode(bytes);
    QCOMPARE(decoded.status, Charging::DecodeStatus::Complete);
    QCOMPARE(decoded.message.header.messageType, MessageType::AdminCommandResponse);
    QCOMPARE(decoded.message.header.requestId, quint32(201));
    if (delta <= 0) {
        QCOMPARE(decoded.message.header.payloadLength, quint32(Charging::MessageHeader::MaxPayloadLength + delta));
        QCOMPARE(decoded.message.header.statusCode, ErrorCode(status));
        QCOMPARE(decoded.message.payload, payload);
    } else {
        QCOMPARE(decoded.message.header.statusCode, ErrorCode::InternalError);
        QCOMPARE(decoded.message.payload.value("reason").toString(), QString("response_too_large"));
        QVERIFY(!decoded.message.payload.value("message").toString().isEmpty());
        QVERIFY(decoded.message.header.payloadLength < 1024);
    }
}

void BusinessIntegrationTest::directPaddedAdminRequestIsBounded()
{
    DatabaseManager database;
    ServiceRegistry services;
    QString error;
    QVERIFY2(database.open(m_databasePath, &error), qPrintable(error));
    QVERIFY2(services.initialize(&database, &error), qPrintable(error));
    MessageDispatcher dispatcher(&services);
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    session.authenticate(Charging::Role::Administrator, 1, "admin");
    for (const auto spare : {4096, 0}) {
        const auto payload = paddedAdminPayload(Charging::MessageHeader::MaxPayloadLength - spare);
        auto incoming = Charging::PacketCodec::encode(MessageType::AdminCommandRequest, 202, payload);
        const auto request = Charging::PacketCodec::tryDecode(incoming);
        QCOMPARE(request.status, Charging::DecodeStatus::Complete);
        QCOMPARE(request.message.header.payloadLength, quint32(Charging::MessageHeader::MaxPayloadLength - spare));
        socket->writes.clear();
        dispatcher.dispatch(&session, request.message);
        QTRY_COMPARE_WITH_TIMEOUT(socket->writes.size(), 1, 3000);
        auto outgoing = socket->writes.first();
        const auto response = Charging::PacketCodec::tryDecode(outgoing);
        QCOMPARE(response.status, Charging::DecodeStatus::Complete);
        QCOMPARE(response.message.header.messageType, MessageType::AdminCommandResponse);
        QCOMPARE(response.message.header.requestId, quint32(202));
        if (spare) {
            QCOMPARE(response.message.header.statusCode, ErrorCode::Success);
            QCOMPARE(response.message.payload.value("action"), payload.value("action"));
        } else {
            QCOMPARE(response.message.header.statusCode, ErrorCode::InternalError);
            QCOMPARE(response.message.payload.value("reason").toString(), QString("response_too_large"));
            QVERIFY(!response.message.payload.contains("action"));
        }
        QVERIFY(takeRequestSlot(session, 202));
    }
    dispatcher.dispatch(&session, packet(MessageType::Ping, 203));
    QCOMPARE(socket->messages.last().header.messageType, MessageType::Pong);
}

void BusinessIntegrationTest::directOversizedPushIsRejected()
{
    auto *socket = new RecordingSocket;
    ClientSession session(socket);
    const QJsonObject oversized{{"message", QString(Charging::MessageHeader::MaxPayloadLength, 'x')}};
    for (const auto type : {MessageType::ChargingProgressPush, MessageType::ChargingStoppedPush,
                            MessageType::AlarmPush, MessageType::DeviceStatusPush}) {
        QVERIFY(!session.send(type, 0, oversized));
        QVERIFY(socket->writes.isEmpty());
    }
}

void BusinessIntegrationTest::paddedAdminRequestIsBounded()
{
    ClientConnection admin;
    loginAdmin(admin);
    for (const auto spare : {4096, 0}) {
        const auto payload = paddedAdminPayload(Charging::MessageHeader::MaxPayloadLength - spare);
        const auto result = request(admin, MessageType::AdminCommandRequest, MessageType::AdminCommandResponse,
                                    payload, spare ? ErrorCode::Success : ErrorCode::InternalError, 204);
        QVERIFY(result.header.payloadLength <= Charging::MessageHeader::MaxPayloadLength);
        if (!spare) QCOMPARE(result.payload.value("reason").toString(), QString("response_too_large"));
    }
    request(admin, MessageType::Ping, MessageType::Pong);
}

void BusinessIntegrationTest::oversizedChargingPushIsAudited()
{
    {
        ClientConnection user;
        login(user);
        request(user, MessageType::WalletRechargeRequest, MessageType::WalletRechargeResponse,
                {{"amountCents", 10000}, {"transactionId", "oversized-push"}});
        const auto reservation = request(user, MessageType::ReservationCreateRequest, MessageType::ReservationCreateResponse,
                                         {{"stationId", 1}, {"pileId", 1}}).payload;
        request(user, MessageType::ChargingStartRequest, MessageType::ChargingStartResponse,
                {{"reservationId", reservation.value("reservationId")}});
    }
    m_server.reset();
    // Restoring a real persisted order exercises the normal tick/delivery/audit
    // path with oversized device/order data, without exposing a test API.
    const auto started = QDateTime::currentDateTimeUtc().addSecs(-3).toString(Qt::ISODate);
    sql(QString("UPDATE charging_orders SET order_no='%1', started_at='%2' WHERE status='charging'")
        .arg(QString(Charging::MessageHeader::MaxPayloadLength, 'x'), started));
    m_server = std::make_unique<ServerApplication>();
    QString error;
    QVERIFY2(m_server->start(0, m_databasePath, &error), qPrintable(error));
    ClientConnection user, admin;
    login(user);
    loginAdmin(admin);
    QSignalSpy userMessages(&user, &ClientConnection::messageReceived), adminMessages(&admin, &ClientConnection::messageReceived);
    triggerJob("chargingTick");
    QTRY_VERIFY_WITH_TIMEOUT(sql("SELECT COUNT(*) FROM push_records WHERE result='failed'").toInt() >= 2, 3000);
    QVERIFY(sql("SELECT COUNT(*) FROM push_records WHERE result='failed' AND target_role='user'").toInt() >= 1);
    QVERIFY(sql("SELECT COUNT(*) FROM push_records WHERE result='failed' AND target_role='administrator'").toInt() >= 1);
    QVERIFY(userMessages.isEmpty());
    QVERIFY(adminMessages.isEmpty());
    QCOMPARE(sql("SELECT status FROM charging_orders").toString(), QString("charging"));
    QVERIFY(sql("SELECT energy_wh FROM charging_orders").toInt() > 0);
    request(user, MessageType::UserProfileRequest, MessageType::UserProfileResponse);
}

void BusinessIntegrationTest::directPeerStreamRejectsReordering()
{
    ClientConnection peer;
    QSignalSpy messages(&peer, &ClientConnection::messageReceived);
    const auto first = packet(MessageType::ChargingProgressPush, 0,
                              {{"orderId", 1}, {"seq", 1}, {"durationSec", 0}});
    const auto second = packet(MessageType::ChargingProgressPush, 0,
                               {{"orderId", 1}, {"seq", 2}, {"durationSec", 1}});
    // Exercise the same consumer used by the TCP workflow with actual signal
    // arrival order 2,1. Searching ahead for seq 1 would incorrectly accept it.
    peer.messageReceived(second);
    peer.messageReceived(first);
    qint64 previousSequence = 0;
    const auto error = compareNextPeerFrame(messages, first, &previousSequence, 100);
    QVERIFY2(!error.isEmpty(), "Peer stream 2,1 must fail the workflow's stream comparison");
    // The remaining seq 1 must also fail the consecutive check after seq 2,
    // even when its payload happens to match the requested owner snapshot.
    previousSequence = 2;
    QVERIFY(!compareNextPeerFrame(messages, first, &previousSequence, 100).isEmpty());
    // A repeated frame must fail even when it matches the owner's payload.
    peer.messageReceived(first);
    previousSequence = 1;
    QVERIFY(!compareNextPeerFrame(messages, first, &previousSequence, 100).isEmpty());
}

void BusinessIntegrationTest::directPeerStreamAcceptsZeroSecondAndFinal()
{
    ClientConnection peer;
    QSignalSpy messages(&peer, &ClientConnection::messageReceived);
    const QList<Message> owner{
        packet(MessageType::ChargingProgressPush, 0, {{"orderId", 1}, {"seq", 1}, {"durationSec", 0}}),
        packet(MessageType::ChargingProgressPush, 0, {{"orderId", 1}, {"seq", 2}, {"durationSec", 1}}),
        packet(MessageType::ChargingStoppedPush, 0, {{"orderId", 1}, {"seq", 3}, {"durationSec", 1}})
    };
    for (const auto &frame : owner) peer.messageReceived(frame);
    qint64 previousSequence = 0;
    for (const auto &frame : owner) {
        const auto error = compareNextPeerFrame(messages, frame, &previousSequence, 100);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    QCOMPARE(previousSequence, qint64(3));
    QVERIFY(messages.isEmpty());
}

void BusinessIntegrationTest::directPeerStreamsRejectSharedGap_data()
{
    QTest::addColumn<int>("progressSequence");
    QTest::addColumn<int>("stoppedSequence");
    QTest::newRow("missing-progress-1-3-4") << 3 << 4;
    QTest::newRow("gap-before-stopped-1-2-4") << 2 << 4;
}

void BusinessIntegrationTest::directPeerStreamsRejectSharedGap()
{
    QFETCH(int, progressSequence);
    QFETCH(int, stoppedSequence);
    ClientConnection admin, secondOwner;
    QSignalSpy adminMessages(&admin, &ClientConnection::messageReceived);
    QSignalSpy secondMessages(&secondOwner, &ClientConnection::messageReceived);
    const QList<Message> owner{
        packet(MessageType::ChargingProgressPush, 0, {{"orderId", 1}, {"seq", 1}, {"durationSec", 0}}),
        packet(MessageType::ChargingProgressPush, 0, {{"orderId", 1}, {"seq", progressSequence}, {"durationSec", 1}}),
        packet(MessageType::ChargingStoppedPush, 0, {{"orderId", 1}, {"seq", stoppedSequence}, {"durationSec", 1}})
    };
    // Every recipient has the same omission: payload equality across peers
    // cannot detect the missing event unless each stream enforces +1.
    for (const auto &frame : owner) {
        admin.messageReceived(frame);
        secondOwner.messageReceived(frame);
    }
    qint64 adminSequence = 0, secondSequence = 0;
    QString adminError, secondError;
    for (const auto &frame : owner) {
        if (adminError.isEmpty()) adminError = compareNextPeerFrame(adminMessages, frame, &adminSequence, 100);
        if (secondError.isEmpty()) secondError = compareNextPeerFrame(secondMessages, frame, &secondSequence, 100);
    }
    QVERIFY2(!adminError.isEmpty(), "Administrator stream must reject a shared sequence gap");
    QVERIFY2(!secondError.isEmpty(), "Second-owner stream must reject a shared sequence gap");
}

QTEST_GUILESS_MAIN(BusinessIntegrationTest)
