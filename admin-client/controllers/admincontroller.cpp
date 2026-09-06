#include "controllers/admincontroller.h"

#include "protocol/errorcodes.h"

#include <QDateTime>
#include <QList>
#include <QStringList>

AdminController::AdminController(QObject *parent) : QObject(parent) {
  m_connection.setClientName(QStringLiteral("admin-client"));
  m_requestTimeoutTimer.setInterval(250);
  connect(&m_requestTimeoutTimer, &QTimer::timeout, this,
          &AdminController::expireRequests);
  m_requestTimeoutTimer.start();

  connect(&m_connection, &Charging::ClientConnection::connected, this, [this] {
    emit statusTextChanged(QStringLiteral("已连接后台服务器"), true);
    sendPendingLogin();
  });
  connect(&m_connection, &Charging::ClientConnection::disconnected, this,
          [this] {
            emit statusTextChanged(
                QStringLiteral("后台服务器连接已断开，等待自动重连"), false);
            failPendingRequests(QStringLiteral("连接已断开，请稍后重试"),
                                Charging::ErrorCode::NetworkUnavailable);
            if (m_loggedIn) {
              m_loggedIn = false;
              emit loginFailed(
                  QStringLiteral("连接中断，请重新输入管理员密码登录"));
              emit loggedOut();
            }
          });
  connect(&m_connection, &Charging::ClientConnection::socketError, this,
          [this](const QString &message) {
            emit statusTextChanged(QStringLiteral("连接失败：%1").arg(message),
                                   false);
          });
  connect(&m_connection, &Charging::ClientConnection::reconnectScheduled, this,
          [this](int attempt, int delay) {
            emit statusTextChanged(QStringLiteral("第%1次重连将在%2秒后进行")
                                       .arg(attempt)
                                       .arg(delay / 1000.0, 0, 'f', 1),
                                   false);
          });
  connect(&m_connection, &Charging::ClientConnection::messageReceived, this,
          &AdminController::handleMessage);
}

AdminController::~AdminController() {
  m_requestTimeoutTimer.stop();
  disconnect(&m_connection, nullptr, this, nullptr);
  m_connection.setAutoReconnect(false);
  m_connection.disconnectFromServer();
}

bool AdminController::isLoggedIn() const { return m_loggedIn; }

void AdminController::setRequestTimeoutMilliseconds(int milliseconds) {
  m_requestTimeoutMilliseconds = qMax(100, milliseconds);
}

void AdminController::connectToServer(const QString &host, quint16 port) {
  emit statusTextChanged(QStringLiteral("正在连接后台服务器…"), false);
  m_connection.connectToServer(host, port);
}

void AdminController::login(const QString &username, const QString &password,
                            const QString &host, quint16 port) {
  m_pendingUsername = username.trimmed();
  m_pendingPassword = password;
  emit loginBusyChanged(true);
  if (m_connection.isConnected()) {
    sendPendingLogin();
  } else {
    connectToServer(host, port);
  }
}

void AdminController::logout() {
  failPendingRequests(QStringLiteral("管理员已退出登录"),
                      Charging::ErrorCode::SessionExpired);
  if (m_connection.isConnected() && m_loggedIn) {
    m_connection.send(Charging::MessageType::LogoutRequest,
                      m_connection.nextRequestId());
    return;
  }
  m_loggedIn = false;
  emit loggedOut();
}

void AdminController::requestAdminCommand(const QString &action,
                                          const QJsonObject &parameters) {
  const QString normalizedAction = action.trimmed();
  if (normalizedAction.isEmpty()) {
    emit commandFailed(action, QStringLiteral("管理员请求动作不能为空"),
                       static_cast<int>(Charging::ErrorCode::ValidationFailed));
    return;
  }
  if (!m_loggedIn) {
    emit commandFailed(normalizedAction,
                       QStringLiteral("管理员登录已失效，请重新登录"),
                       static_cast<int>(Charging::ErrorCode::Unauthorized));
    return;
  }
  if (!m_connection.isConnected()) {
    emit commandFailed(
        normalizedAction, QStringLiteral("服务器连接不可用，请稍后重试"),
        static_cast<int>(Charging::ErrorCode::NetworkUnavailable));
    return;
  }

  // 页面仍使用早期原型中的动作名。这里统一翻译成服务端最终协议，
  // 同时在 PendingRequest 中保留原动作名，保证页面的加载态和回调无需改写。
  QString wireAction = normalizedAction;
  QJsonObject payload = parameters;
  if (normalizedAction == QStringLiteral("report.summary")) {
    wireAction = QStringLiteral("dashboard.summary");
  } else if (normalizedAction == QStringLiteral("report.pileStates")) {
    wireAction = QStringLiteral("pile.status.summary");
  } else if (normalizedAction == QStringLiteral("admin.monitor")) {
    wireAction = QStringLiteral("charging.active.list");
  } else if (normalizedAction == QStringLiteral("admin.alarms")) {
    wireAction = QStringLiteral("alarm.list");
    if (payload.value(QStringLiteral("recovered")).toBool())
      payload.insert(QStringLiteral("status"), QStringLiteral("resolved"));
    else if (payload.value(QStringLiteral("handled")).toBool())
      payload.insert(QStringLiteral("status"), QStringLiteral("acknowledged"));
    else if (payload.contains(QStringLiteral("handled")) ||
             payload.contains(QStringLiteral("recovered")))
      payload.insert(QStringLiteral("status"), QStringLiteral("open"));
    payload.remove(QStringLiteral("handled"));
    payload.remove(QStringLiteral("recovered"));
  } else if (normalizedAction == QStringLiteral("admin.stations")) {
    wireAction = QStringLiteral("station.list");
  } else if (normalizedAction == QStringLiteral("admin.stationDetail")) {
    wireAction = QStringLiteral("station.detail");
  } else if (normalizedAction == QStringLiteral("admin.piles")) {
    wireAction = QStringLiteral("pile.list");
  } else if (normalizedAction == QStringLiteral("admin.users")) {
    wireAction = QStringLiteral("user.list");
    payload.insert(QStringLiteral("phoneKeyword"),
                   payload.take(QStringLiteral("phoneContains")));
  } else if (normalizedAction == QStringLiteral("orders.list")) {
    wireAction = QStringLiteral("order.list");
  } else if (normalizedAction == QStringLiteral("admin.pileControl")) {
    wireAction = QStringLiteral("piles.control");
    payload.insert(QStringLiteral("command"),
                   payload.take(QStringLiteral("action"))
                       .toString()
                       .trimmed()
                       .toLower());
  } else if (normalizedAction == QStringLiteral("admin.saveStation")) {
    const QJsonObject input = payload.take(QStringLiteral("input")).toObject();
    for (auto it = input.constBegin(); it != input.constEnd(); ++it)
      payload.insert(it.key(), it.value());
    wireAction = payload.contains(QStringLiteral("stationId"))
                     ? QStringLiteral("station.update")
                     : QStringLiteral("station.create");
  } else if (normalizedAction == QStringLiteral("admin.setUserStatus")) {
    wireAction = QStringLiteral("users.freeze");
    payload.insert(
        QStringLiteral("frozen"),
        payload.value(QStringLiteral("status"))
                .toString()
                .compare(QStringLiteral("FROZEN"), Qt::CaseInsensitive) == 0);
    payload.remove(QStringLiteral("status"));
  }

  const quint32 previousRequestId =
      m_latestRequestByAction.value(normalizedAction, 0);
  const quint32 requestId = m_connection.nextRequestId();
  payload.insert(QStringLiteral("action"), wireAction);

  if (previousRequestId != 0) {
    m_pendingRequests.remove(previousRequestId);
  }
  PendingRequest pending;
  pending.action = normalizedAction;
  pending.deadline =
      QDateTime::currentMSecsSinceEpoch() + m_requestTimeoutMilliseconds;
  m_pendingRequests.insert(requestId, pending);
  m_latestRequestByAction.insert(normalizedAction, requestId);
  emit commandBusyChanged(normalizedAction, true);

  if (!m_connection.send(Charging::MessageType::AdminCommandRequest, requestId,
                         payload)) {
    m_pendingRequests.remove(requestId);
    if (m_latestRequestByAction.value(normalizedAction) == requestId) {
      m_latestRequestByAction.remove(normalizedAction);
    }
    emit commandBusyChanged(normalizedAction, false);
    emit commandFailed(
        normalizedAction, QStringLiteral("请求发送失败"),
        static_cast<int>(Charging::ErrorCode::NetworkUnavailable));
    return;
  }
}

void AdminController::sendPendingLogin() {
  if (m_pendingUsername.isEmpty() || m_pendingPassword.isEmpty() ||
      !m_connection.isConnected())
    return;
  m_loginRequestId = m_connection.nextRequestId();
  if (!m_connection.send(Charging::MessageType::AdminLoginRequest,
                         m_loginRequestId,
                         {{QStringLiteral("username"), m_pendingUsername},
                          {QStringLiteral("password"), m_pendingPassword}})) {
    emit loginBusyChanged(false);
  }
}

void AdminController::handleMessage(const Charging::Message &message) {
  if (message.header.messageType == Charging::MessageType::AdminLoginResponse &&
      message.header.requestId == m_loginRequestId) {
    emit loginBusyChanged(false);
    m_pendingPassword.clear();
    if (message.header.statusCode != Charging::ErrorCode::Success) {
      m_loggedIn = false;
      emit loginFailed(Charging::errorMessage(
          message.header.statusCode,
          message.payload.value(QStringLiteral("message")).toString()));
      return;
    }
    m_loggedIn = true;
    m_pendingUsername.clear();
    emit loginSucceeded(message.payload);
    return;
  }

  if (message.header.messageType == Charging::MessageType::LogoutResponse) {
    m_loggedIn = false;
    failPendingRequests(QStringLiteral("管理员已退出登录"),
                        Charging::ErrorCode::SessionExpired);
    emit loggedOut();
    return;
  }

  const bool legacyCommandError =
      message.header.messageType ==
          Charging::MessageType::AdminCommandRequest &&
      message.header.statusCode != Charging::ErrorCode::Success;
  if (message.header.messageType ==
          Charging::MessageType::AdminCommandResponse ||
      legacyCommandError) {
    const auto pendingIt = m_pendingRequests.find(message.header.requestId);
    if (pendingIt == m_pendingRequests.end()) {
      return;
    }
    const QString action = pendingIt->action;
    m_pendingRequests.erase(pendingIt);
    if (m_latestRequestByAction.value(action) != message.header.requestId) {
      return;
    }
    m_latestRequestByAction.remove(action);
    emit commandBusyChanged(action, false);
    if (message.header.statusCode == Charging::ErrorCode::Success) {
      emit commandSucceeded(action, message.payload);
    } else {
      const QString error = Charging::errorMessage(
          message.header.statusCode,
          message.payload.value(QStringLiteral("message")).toString());
      emit commandFailed(action, error,
                         static_cast<int>(message.header.statusCode));
      if (message.header.statusCode == Charging::ErrorCode::Unauthorized ||
          message.header.statusCode == Charging::ErrorCode::SessionExpired) {
        m_loggedIn = false;
        failPendingRequests(error, message.header.statusCode);
        emit loggedOut();
      }
    }
    return;
  }

  switch (message.header.messageType) {
  case Charging::MessageType::ChargingProgressPush:
  case Charging::MessageType::ChargingStoppedPush:
  case Charging::MessageType::AlarmPush:
  case Charging::MessageType::DeviceStatusPush:
    emit pushReceived(static_cast<quint16>(message.header.messageType),
                      message.payload);
    break;
  default:
    break;
  }
}

void AdminController::expireRequests() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QList<quint32> expiredIds;
  for (auto it = m_pendingRequests.cbegin(); it != m_pendingRequests.cend();
       ++it) {
    if (it->deadline <= now) {
      expiredIds.append(it.key());
    }
  }
  for (quint32 requestId : expiredIds) {
    const PendingRequest pending = m_pendingRequests.take(requestId);
    if (m_latestRequestByAction.value(pending.action) != requestId) {
      continue;
    }
    m_latestRequestByAction.remove(pending.action);
    emit commandBusyChanged(pending.action, false);
    emit commandFailed(pending.action, QStringLiteral("请求超时，请稍后重试"),
                       static_cast<int>(Charging::ErrorCode::RequestTimeout));
  }
}

void AdminController::failPendingRequests(const QString &message,
                                          Charging::ErrorCode errorCode) {
  const QStringList actions = m_latestRequestByAction.keys();
  m_pendingRequests.clear();
  m_latestRequestByAction.clear();
  for (const QString &action : actions) {
    emit commandBusyChanged(action, false);
    emit commandFailed(action, message, static_cast<int>(errorCode));
  }
}
