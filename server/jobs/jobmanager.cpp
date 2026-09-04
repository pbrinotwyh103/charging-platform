#include "jobs/jobmanager.h"

JobManager::JobManager(QObject *parent)
    : QObject(parent)
{
    m_heartbeatTimer.setInterval(10'000);
    m_chargingTimer.setInterval(1'000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &JobManager::heartbeatTick);
    connect(&m_chargingTimer, &QTimer::timeout, this, &JobManager::chargingTick);
}

void JobManager::start()
{
    m_heartbeatTimer.start();
    m_chargingTimer.start();
}
