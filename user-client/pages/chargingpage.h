#pragma once

#include <QWidget>
#include <QJsonObject>

class QLabel;
class QTimer;

class ChargingPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ChargingPage(QWidget *parent = nullptr);
    void setSnapshot(const QJsonObject &snapshot);
    void setDisconnected(bool disconnected);

private:
    void refreshExpiryState();
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_expiryTimer = nullptr;
    QJsonObject m_snapshot;
    qint64 m_lastSnapshotMs = 0;
    bool m_disconnected = false;
};
