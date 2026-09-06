#include "stores/snapshotstore.h"
#include <QDateTime>
bool SnapshotStore::apply(const QJsonObject &s)
{
    const QString orderId =
        s.value(QStringLiteral("orderId")).toVariant().toString();
    const qint64 seq = static_cast<qint64>(s.value(QStringLiteral("seq")).toDouble(-1));
    if (orderId.isEmpty() || seq < 0) return false;
    if (!orderId.isEmpty() && orderId != m_orderId) { m_orderId = orderId; m_seq = -1; }
    if (seq <= m_seq) return false;
    m_seq = seq; m_updatedAt = QDateTime::currentMSecsSinceEpoch(); m_snapshot = s; emit updated(s); emit expiredChanged(false); return true;
}
bool SnapshotStore::expired() const { return m_updatedAt == 0 || QDateTime::currentMSecsSinceEpoch() - m_updatedAt > 10000; }
