#pragma once
#include <QObject>
#include <QJsonObject>

class SnapshotStore final : public QObject
{
    Q_OBJECT
public:
    explicit SnapshotStore(QObject *parent = nullptr) : QObject(parent) {}
    bool apply(const QJsonObject &snapshot);
    QJsonObject current() const { return m_snapshot; }
    bool expired() const;
signals:
    void updated(const QJsonObject &);
    void expiredChanged(bool);
private:
    QJsonObject m_snapshot;
    qint64 m_seq = -1;
    QString m_orderId;
    qint64 m_updatedAt = 0;
};
