#include "app/serverapplication.h"
#include "repositories/pushrecordrepository.h"
#include "services/servicehelpers.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QtConcurrent>

ServerApplication::ServerApplication(QObject *parent)
    : QObject(parent),
      m_dispatcher(&m_services, this),
      m_server(this),
      m_jobs(this)
{
    m_jobPool.setMaxThreadCount(1);
    connect(&m_server, &TcpServer::messageReceived,
            &m_dispatcher, &MessageDispatcher::dispatch);
    connect(&m_jobs, &JobManager::heartbeatTick, this, [this] {
        m_server.closeExpiredSessions(45'000);
    });
    connect(&m_jobs, &JobManager::chargingTick, this, &ServerApplication::sampleCharging);
    connect(&m_jobs, &JobManager::reservationExpiryTick, this, &ServerApplication::expireReservations);
}

ServerApplication::~ServerApplication()
{
    // Database/service members must outlive every background job.
    m_jobPool.waitForDone();
}

void ServerApplication::sampleCharging()
{
    if (!m_services.isInitialized() || m_chargingTickRunning) return;
    m_chargingTickRunning = true;
    auto *watcher = new QFutureWatcher<QList<ChargingEvent>>(this);
    connect(watcher, &QFutureWatcher<QList<ChargingEvent>>::finished, this, [this, watcher] {
        const auto events = watcher->result();
        watcher->deleteLater();
        deliverEvents(events);
        m_chargingTickRunning = false;
    });
    watcher->setFuture(QtConcurrent::run(&m_jobPool, [this] {
        return m_services.charging()->tick(QDateTime::currentDateTimeUtc());
    }));
}

void ServerApplication::expireReservations()
{
    if (!m_services.isInitialized() || m_reservationExpiryRunning) return;
    m_reservationExpiryRunning = true;
    auto *watcher = new QFutureWatcher<ServiceResult>(this);
    connect(watcher, &QFutureWatcher<ServiceResult>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_reservationExpiryRunning = false;
        if (!result.succeeded())
            qWarning().noquote() << QStringLiteral("预约过期任务失败：%1")
                .arg(Charging::errorMessage(result.error, result.message));
    });
    watcher->setFuture(QtConcurrent::run(&m_jobPool, [this] {
        return m_services.reservations()->expireDue(QDateTime::currentDateTimeUtc());
    }));
}

void ServerApplication::deliverEvents(const QList<ChargingEvent> &events)
{
    struct Failure { QString role; qint64 principalId; int messageType; };
    QList<Failure> failures;
    // One tick remains in flight until delivery completes, preserving each
    // order's event order even while dispatcher workers start/stop charging.
    for (const auto &event : events) {
        auto targets = m_server.sessionsForUser(event.userId);
        targets.append(m_server.administratorSessions());
        for (auto *session : targets) {
            if (!session->send(event.type, 0, event.payload))
                failures.append({session->role() == Charging::Role::User ? "user" : "administrator",
                                 session->principalId(), int(event.type)});
        }
    }
    if (failures.isEmpty()) return;
    (void)QtConcurrent::run(&m_jobPool, [this, failures] {
        ServiceHelpers::ConnectionCleanup cleanup(&m_database);
        PushRecordRepository repository(&m_database);
        for (const auto &failure : failures) {
            QString error;
            if (!repository.insert(failure.role, failure.principalId, failure.messageType,
                                   0, "failed", nullptr, &error))
                qWarning().noquote() << QStringLiteral("推送失败记录写入失败：%1").arg(error);
        }
    });
}

bool ServerApplication::start(quint16 port, const QString &databasePath, QString *error)
{
    if (!m_database.open(databasePath, error)) {
        return false;
    }
    if (!m_services.initialize(&m_database, error)) {
        return false;
    }
    if (!m_server.listen(port, error)) {
        return false;
    }
    m_jobs.start();
    qInfo().noquote() << QStringLiteral("服务器框架已启动，端口=%1，数据库=%2")
                         .arg(m_server.listeningPort()).arg(databasePath);
    return true;
}

quint16 ServerApplication::listeningPort() const
{
    return m_server.listeningPort();
}
