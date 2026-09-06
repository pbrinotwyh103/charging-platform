#pragma once

#include <QWidget>
#include <QJsonObject>

class QLabel;
class QTimer;
class QPushButton;

class ChargingPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ChargingPage(QWidget *parent = nullptr);
    void setSnapshot(const QJsonObject &snapshot);
    void setReservation(const QJsonObject &reservation);
    void setNoActiveTask();
    void setDisconnected(bool disconnected);
    void showActionError(const QString &message);

signals:
    void startRequested(qint64 reservationId);
    void cancelRequested(qint64 reservationId);
    void stopRequested(qint64 orderId);

private:
    void refreshExpiryState();
    void updateActions();
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_actionStatusLabel = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QTimer *m_expiryTimer = nullptr;
    QJsonObject m_snapshot;
    QJsonObject m_reservation;
    qint64 m_lastSnapshotMs = 0;
    bool m_disconnected = false;
    bool m_actionPending = false;
};
