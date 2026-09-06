#pragma once

#include <QObject>
#include <QTimer>

class JobManager final : public QObject
{
    Q_OBJECT

public:
    explicit JobManager(QObject *parent = nullptr);
    void start();

signals:
    void heartbeatTick();
    void chargingTick();
    void reservationExpiryTick();

private:
    QTimer m_heartbeatTimer;
    QTimer m_chargingTimer;
    QTimer m_reservationExpiryTimer;
};
