#include "jobs/jobmanager.h"

JobManager::JobManager(QObject *parent)
    : QObject(parent)
{
    m_heartbeatTimer.setInterval(10'000);
    m_chargingTimer.setInterval(1'000);
    m_reservationExpiryTimer.setInterval(60'000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &JobManager::heartbeatTick);
    connect(&m_chargingTimer, &QTimer::timeout, this, &JobManager::chargingTick);
    connect(&m_reservationExpiryTimer, &QTimer::timeout, this, &JobManager::reservationExpiryTick);
}

void JobManager::start()
{
    m_heartbeatTimer.start();
    m_chargingTimer.start();
    m_reservationExpiryTimer.start();
}
