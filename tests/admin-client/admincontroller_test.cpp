#include "controllers/admincontroller.h"

#include "protocol/packetcodec.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class FakeAdminServer final : public QObject {
  Q_OBJECT

public:
  explicit FakeAdminServer(QObject *parent = nullptr) : QObject(parent) {
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
      m_client = m_server.nextPendingConnection();
      connect(m_client, &QTcpSocket::readyRead, this,
              &FakeAdminServer::readClientMessages);
    });
  }

  bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }

  quint16 port() const { return m_server.serverPort(); }

  int commandCount() const { return m_commands.size(); }

  Charging::Message commandAt(int index) const { return m_commands.at(index); }

  void setAutomaticCommandResponses(bool enabled) {
    m_automaticCommandResponses = enabled;
  }

  void
  respondToCommand(int index, const QJsonObject &data = {},
                   Charging::ErrorCode status = Charging::ErrorCode::Success) {
    const Charging::Message command = m_commands.at(index);
    const QString action =
        command.payload.value(QStringLiteral("action")).toString();
    QJsonObject payload = {{QStringLiteral("action"), action},
                           {QStringLiteral("data"), data}};
    if (data.contains(QStringLiteral("message"))) {
      payload.insert(QStringLiteral("message"),
                     data.value(QStringLiteral("message")));
    }
    send(Charging::MessageType::AdminCommandResponse, command.header.requestId,
         payload, status);
  }

  void sendPush(Charging::MessageType type, const QJsonObject &payload) {
    send(type, 0, payload);
  }

  void disconnectClient() {
    if (m_client) {
      m_client->disconnectFromHost();
    }
  }

private:
  void readClientMessages() {
    m_buffer.append(m_client->readAll());
    while (!m_buffer.isEmpty()) {
      const Charging::DecodeResult result =
          Charging::PacketCodec::tryDecode(m_buffer);
      if (result.status == Charging::DecodeStatus::NeedMoreData)
        return;
      if (result.status == Charging::DecodeStatus::Invalid) {
        m_buffer.clear();
        return;
      }
      handleMessage(result.message);
    }
  }

  void handleMessage(const Charging::Message &message) {
    if (message.header.messageType ==
        Charging::MessageType::AdminLoginRequest) {
      send(Charging::MessageType::AdminLoginResponse, message.header.requestId,
           {{QStringLiteral("adminId"), 1},
            {QStringLiteral("username"), QStringLiteral("admin")},
            {QStringLiteral("permissions"), QStringLiteral("all")},
            {QStringLiteral("role"), QStringLiteral("administrator")}});
      return;
    }
    if (message.header.messageType ==
        Charging::MessageType::AdminCommandRequest) {
      m_commands.append(message);
      if (m_automaticCommandResponses) {
        respondToCommand(m_commands.size() - 1,
                         {{QStringLiteral("accepted"), true}});
      }
      return;
    }
    if (message.header.messageType == Charging::MessageType::Ping) {
      send(Charging::MessageType::Pong, message.header.requestId);
    }
  }

  void send(Charging::MessageType type, quint32 requestId,
            const QJsonObject &payload = {},
            Charging::ErrorCode status = Charging::ErrorCode::Success) {
    if (m_client) {
      m_client->write(
          Charging::PacketCodec::encode(type, requestId, payload, status));
      m_client->flush();
    }
  }

  QTcpServer m_server;
  QTcpSocket *m_client = nullptr;
  QByteArray m_buffer;
  QList<Charging::Message> m_commands;
  bool m_automaticCommandResponses = true;
};

class AdminControllerTest final : public QObject {
  Q_OBJECT

private slots:
  void unauthenticatedCommandIsRejected();
  void loginAndCommandRoundTrip();
  void staleResponseCannotOverrideLatestRequest();
  void serviceErrorCodeIsPreserved();
  void sessionExpiryInvalidatesSessionAndPendingCommands();
  void commandTimeoutIsReported();
  void disconnectionFailsPendingCommand();
  void serverPushIsForwarded();

private:
  void login(AdminController &controller, FakeAdminServer &server);
};

void AdminControllerTest::login(AdminController &controller,
                                FakeAdminServer &server) {
  QVERIFY(server.listen());
  QSignalSpy loginSucceeded(&controller, &AdminController::loginSucceeded);
  controller.login(QStringLiteral("admin"), QStringLiteral("123456"),
                   QStringLiteral("127.0.0.1"), server.port());
  QTRY_COMPARE_WITH_TIMEOUT(loginSucceeded.count(), 1, 3'000);
  QVERIFY(controller.isLoggedIn());
}

void AdminControllerTest::unauthenticatedCommandIsRejected() {
  AdminController controller;
  QSignalSpy failed(&controller, &AdminController::commandFailed);
  controller.requestAdminCommand(QStringLiteral("dashboard.summary"));
  QCOMPARE(failed.count(), 1);
  QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("dashboard.summary"));
  QCOMPARE(failed.at(0).at(2).toInt(),
           static_cast<int>(Charging::ErrorCode::Unauthorized));
}

void AdminControllerTest::loginAndCommandRoundTrip() {
  FakeAdminServer server;
  AdminController controller;
  login(controller, server);

  QSignalSpy busy(&controller, &AdminController::commandBusyChanged);
  QSignalSpy succeeded(&controller, &AdminController::commandSucceeded);
  controller.requestAdminCommand(
      QStringLiteral("dashboard.summary"),
      {{QStringLiteral("scope"), QStringLiteral("all")}});
  QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 3'000);
  QCOMPARE(server.commandCount(), 1);
  QCOMPARE(
      server.commandAt(0).payload.value(QStringLiteral("action")).toString(),
      QStringLiteral("dashboard.summary"));
  QCOMPARE(
      server.commandAt(0).payload.value(QStringLiteral("scope")).toString(),
      QStringLiteral("all"));
  QCOMPARE(succeeded.at(0).at(0).toString(),
           QStringLiteral("dashboard.summary"));
  QCOMPARE(busy.count(), 2);
  QCOMPARE(busy.at(0).at(1).toBool(), true);
  QCOMPARE(busy.at(1).at(1).toBool(), false);
}

void AdminControllerTest::staleResponseCannotOverrideLatestRequest() {
  FakeAdminServer server;
  server.setAutomaticCommandResponses(false);
  AdminController controller;
  login(controller, server);

  QSignalSpy succeeded(&controller, &AdminController::commandSucceeded);
  controller.requestAdminCommand(QStringLiteral("users.list"),
                                 {{QStringLiteral("page"), 1}});
  controller.requestAdminCommand(QStringLiteral("users.list"),
                                 {{QStringLiteral("page"), 2}});
  QTRY_COMPARE_WITH_TIMEOUT(server.commandCount(), 2, 3'000);

  server.respondToCommand(1, {{QStringLiteral("page"), 2}});
  server.respondToCommand(0, {{QStringLiteral("page"), 1}});
  QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 3'000);
  const QJsonObject payload = qvariant_cast<QJsonObject>(succeeded.at(0).at(1));
  QCOMPARE(payload.value(QStringLiteral("data"))
               .toObject()
               .value(QStringLiteral("page"))
               .toInt(),
           2);
  QTest::qWait(50);
  QCOMPARE(succeeded.count(), 1);
}

void AdminControllerTest::serviceErrorCodeIsPreserved() {
  FakeAdminServer server;
  server.setAutomaticCommandResponses(false);
  AdminController controller;
  login(controller, server);

  QSignalSpy failed(&controller, &AdminController::commandFailed);
  controller.requestAdminCommand(
      QStringLiteral("piles.control"),
      {{QStringLiteral("pileId"), 7},
       {QStringLiteral("command"), QStringLiteral("restart")}});
  QTRY_COMPARE_WITH_TIMEOUT(server.commandCount(), 1, 3'000);
  server.respondToCommand(
      0, {{QStringLiteral("message"), QStringLiteral("权限不足")}},
      Charging::ErrorCode::Forbidden);
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3'000);
  QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("piles.control"));
  QCOMPARE(failed.at(0).at(2).toInt(),
           static_cast<int>(Charging::ErrorCode::Forbidden));
}

void AdminControllerTest::sessionExpiryInvalidatesSessionAndPendingCommands() {
  FakeAdminServer server;
  server.setAutomaticCommandResponses(false);
  AdminController controller;
  login(controller, server);

  QSignalSpy failed(&controller, &AdminController::commandFailed);
  QSignalSpy loggedOut(&controller, &AdminController::loggedOut);
  controller.requestAdminCommand(QStringLiteral("users.list"));
  controller.requestAdminCommand(QStringLiteral("orders.list"));
  QTRY_COMPARE_WITH_TIMEOUT(server.commandCount(), 2, 3'000);
  server.respondToCommand(
      0, {{QStringLiteral("message"), QStringLiteral("会话已过期")}},
      Charging::ErrorCode::SessionExpired);

  QTRY_COMPARE_WITH_TIMEOUT(loggedOut.count(), 1, 3'000);
  QCOMPARE(failed.count(), 2);
  QVERIFY(!controller.isLoggedIn());
  QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("users.list"));
  QCOMPARE(failed.at(0).at(2).toInt(),
           static_cast<int>(Charging::ErrorCode::SessionExpired));
  QCOMPARE(failed.at(1).at(0).toString(), QStringLiteral("orders.list"));
  QCOMPARE(failed.at(1).at(2).toInt(),
           static_cast<int>(Charging::ErrorCode::SessionExpired));
}

void AdminControllerTest::commandTimeoutIsReported() {
  FakeAdminServer server;
  server.setAutomaticCommandResponses(false);
  AdminController controller;
  controller.setRequestTimeoutMilliseconds(150);
  login(controller, server);

  QSignalSpy failed(&controller, &AdminController::commandFailed);
  controller.requestAdminCommand(QStringLiteral("orders.list"));
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 1'500);
  QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("orders.list"));
  QCOMPARE(failed.at(0).at(2).toInt(),
           static_cast<int>(Charging::ErrorCode::RequestTimeout));
}

void AdminControllerTest::disconnectionFailsPendingCommand() {
  FakeAdminServer server;
  server.setAutomaticCommandResponses(false);
  AdminController controller;
  login(controller, server);

  QSignalSpy failed(&controller, &AdminController::commandFailed);
  controller.requestAdminCommand(QStringLiteral("alarms.list"));
  QTRY_COMPARE_WITH_TIMEOUT(server.commandCount(), 1, 3'000);
  server.disconnectClient();
  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3'000);
  QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("alarms.list"));
  QCOMPARE(failed.at(0).at(2).toInt(),
           static_cast<int>(Charging::ErrorCode::NetworkUnavailable));
}

void AdminControllerTest::serverPushIsForwarded() {
  FakeAdminServer server;
  AdminController controller;
  login(controller, server);

  QSignalSpy pushes(&controller, &AdminController::pushReceived);
  server.sendPush(Charging::MessageType::AlarmPush,
                  {{QStringLiteral("alarmId"), 42},
                   {QStringLiteral("severity"), QStringLiteral("critical")}});
  QTRY_COMPARE_WITH_TIMEOUT(pushes.count(), 1, 3'000);
  QCOMPARE(pushes.at(0).at(0).toUInt(),
           static_cast<uint>(Charging::MessageType::AlarmPush));
  const QJsonObject payload = qvariant_cast<QJsonObject>(pushes.at(0).at(1));
  QCOMPARE(payload.value(QStringLiteral("alarmId")).toInt(), 42);
}

QTEST_GUILESS_MAIN(AdminControllerTest)
#include "admincontroller_test.moc"
