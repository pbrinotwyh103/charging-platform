#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

class ChargingPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ChargingPage(QWidget *parent = nullptr);
    void setSnapshot(const QJsonObject &snapshot);
    void setDisconnected(bool disconnected);
    void setDemoMode(bool enabled);
    void setReservation(const QJsonObject &station, const QJsonObject &pile);
    bool hasActiveOrder() const;

signals:
    void startChargingRequested(qint64 reservationId);
    void stopChargingRequested(qint64 orderId);

private:
    void refreshExpiryState();
    void renderSnapshot();
    void startDemoCharging();
    void stopDemoCharging(const QString &reason = QStringLiteral("用户主动停止"));

    QLabel *m_titleLabel = nullptr;
    QLabel *m_stageLabel = nullptr;
    QLabel *m_orderLabel = nullptr;
    QLabel *m_metricsLabel = nullptr;
    QLabel *m_tipLabel = nullptr;
    QProgressBar *m_batteryProgress = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_faultButton = nullptr;
    QTimer *m_expiryTimer = nullptr;
    QTimer *m_demoTimer = nullptr;
    QJsonObject m_snapshot;
    QJsonObject m_station;
    QJsonObject m_pile;
    qint64 m_lastSnapshotMs = 0;
    qint64 m_orderId = 0;
    int m_durationSec = 0;
    bool m_disconnected = false;
    bool m_demoMode = false;
    bool m_charging = false;
};
