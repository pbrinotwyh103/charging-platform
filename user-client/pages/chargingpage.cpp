#include "pages/chargingpage.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>

ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    m_titleLabel = new QLabel(QStringLiteral("充电"), this);

    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_statusLabel = new QLabel(
        QStringLiteral(
            "当前暂无进行中的充电任务。\n\n"
            "后续将在这里展示：\n"
            "预约状态\n"
            "充电电量\n"
            "实时功率\n"
            "充电时长\n"
            "当前费用\n"
            "最后更新时间"),
        this);

    m_statusLabel->setWordWrap(true);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    m_expiryTimer = new QTimer(this);
    m_expiryTimer->setInterval(1000);
    connect(m_expiryTimer, &QTimer::timeout, this, &ChargingPage::refreshExpiryState);
    m_expiryTimer->start();
}
void ChargingPage::setSnapshot(const QJsonObject &s)
{
    m_snapshot = s;
    m_lastSnapshotMs = QDateTime::currentMSecsSinceEpoch();
    m_disconnected = false;
    m_statusLabel->setText(QStringLiteral("状态：%1\n累计电量：%2 kWh\n实时功率：%3 kW\n充电时长：%4 秒\n当前费用：¥ %5\n最后更新时间：%6")
        .arg(s.value("status").toString()).arg(s.value("energyKwh").toDouble(),0,'f',2)
        .arg(s.value("powerKw").toDouble(),0,'f',1).arg(s.value("durationSec").toInt())
        .arg(s.value("feeCents").toInt()/100.0,0,'f',2).arg(s.value("updatedAt").toString()));
}
void ChargingPage::setDisconnected(bool disconnected)
{
    m_disconnected = disconnected;
    refreshExpiryState();
}

void ChargingPage::refreshExpiryState()
{
    if (m_snapshot.isEmpty()) return;
    const bool expired = m_lastSnapshotMs == 0
        || QDateTime::currentMSecsSinceEpoch() - m_lastSnapshotMs > 10000;
    QString text = QStringLiteral("状态：%1\n累计电量：%2 kWh\n实时功率：%3 kW\n充电时长：%4 秒\n当前费用：¥ %5\n最后更新时间：%6")
        .arg(m_snapshot.value("status").toString())
        .arg(m_snapshot.value("energyKwh").toDouble(), 0, 'f', 2)
        .arg(m_snapshot.value("powerKw").toDouble(), 0, 'f', 1)
        .arg(m_snapshot.value("durationSec").toInt())
        .arg(m_snapshot.value("feeCents").toInt() / 100.0, 0, 'f', 2)
        .arg(m_snapshot.value("updatedAt").toString());
    if (expired) text += QStringLiteral("\n数据已过期（超过 10 秒未收到新快照）");
    if (m_disconnected) text += QStringLiteral("\n网络已断开，以上为最后一次有效数据（非实时）");
    m_statusLabel->setText(text);
}
