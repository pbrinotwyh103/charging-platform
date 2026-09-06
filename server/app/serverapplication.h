#pragma once

#include "database/databasemanager.h"
#include "dispatch/messagedispatcher.h"
#include "jobs/jobmanager.h"
#include "network/tcpserver.h"
#include "services/serviceregistry.h"

#include <QObject>

class ServerApplication final : public QObject
{
    Q_OBJECT

public:
    explicit ServerApplication(QObject *parent = nullptr);
    ~ServerApplication() override;
    bool start(quint16 port, const QString &databasePath, QString *error);
    quint16 listeningPort() const;

private:
    void sampleCharging();
    void expireReservations();
    void deliverEvents(const QList<ChargingEvent> &events);
    DatabaseManager m_database;
    ServiceRegistry m_services;
    MessageDispatcher m_dispatcher;
    TcpServer m_server;
    JobManager m_jobs;
    QThreadPool m_jobPool;
    bool m_chargingTickRunning = false;
    bool m_reservationExpiryRunning = false;
};
