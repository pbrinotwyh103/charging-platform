#include "pages/chargingpage.h"

#include <QDateTime>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("chargingPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_titleLabel = new QLabel(QStringLiteral("充电服务"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_stageLabel = new QLabel(QStringLiteral("暂无进行中的充电任务"), this);
    m_stageLabel->setObjectName(QStringLiteral("chargingStageLabel"));
    m_stageLabel->setStyleSheet(QStringLiteral(
        "padding:10px;background:#f1f5f9;color:#334155;border-radius:8px;font-weight:600;"));
    m_orderLabel = new QLabel(
        QStringLiteral("从首页选择空闲电桩并完成预约后，即可开始充电。"), this);
    m_orderLabel->setWordWrap(true);
    m_metricsLabel = new QLabel(
        QStringLiteral("电量 0.00 kWh\n实时功率 0.0 kW\n已充时长 00:00\n当前费用 ¥0.00"), this);
    m_metricsLabel->setObjectName(QStringLiteral("chargingMetricsLabel"));
    m_metricsLabel->setStyleSheet(QStringLiteral(
        "padding:18px;background:#eff6ff;color:#1e3a8a;border-radius:12px;font-size:16px;"));
    m_batteryProgress = new QProgressBar(this);
    m_batteryProgress->setObjectName(QStringLiteral("batteryProgress"));
    m_batteryProgress->setRange(0, 100);
    m_batteryProgress->setValue(42);
    m_batteryProgress->setFormat(QStringLiteral("车辆电量 %p%"));
    m_tipLabel = new QLabel(QStringLiteral("实时数据以服务端推送为准。"), this);
    m_tipLabel->setWordWrap(true);
    m_tipLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
    m_startButton = new QPushButton(QStringLiteral("等待预约"), this);
    m_startButton->setObjectName(QStringLiteral("startChargingButton"));
    m_startButton->setMinimumHeight(44);
    m_startButton->setEnabled(false);
    m_stopButton = new QPushButton(QStringLiteral("结束充电并结算"), this);
    m_stopButton->setObjectName(QStringLiteral("stopChargingButton"));
    m_stopButton->setMinimumHeight(44);
    m_stopButton->setEnabled(false);
    m_faultButton = new QPushButton(QStringLiteral("演示设备异常保护"), this);
    m_faultButton->setObjectName(QStringLiteral("simulateFaultButton"));
    m_faultButton->setEnabled(false);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_stageLabel);
    layout->addWidget(m_orderLabel);
    layout->addWidget(m_metricsLabel);
    layout->addWidget(m_batteryProgress);
    layout->addWidget(m_tipLabel);
    layout->addStretch();
    layout->addWidget(m_startButton);
    layout->addWidget(m_stopButton);
    layout->addWidget(m_faultButton);

    m_expiryTimer = new QTimer(this);
    m_expiryTimer->setInterval(1000);
    connect(m_expiryTimer, &QTimer::timeout, this, &ChargingPage::refreshExpiryState);
    m_expiryTimer->start();

    m_demoTimer = new QTimer(this);
    m_demoTimer->setInterval(1000);
    connect(m_demoTimer, &QTimer::timeout, this, [this] {
        ++m_durationSec;
        const double energy = 1.86 + m_durationSec * 0.018;
        const double power = 58.0 + (m_durationSec % 5) * 0.7;
        const int feeCents = qRound(energy * 168.0);
        const int battery = qMin(100, 62 + m_durationSec / 2);
        m_batteryProgress->setValue(battery);
        setSnapshot({{QStringLiteral("orderId"), m_orderId},
                     {QStringLiteral("status"), QStringLiteral("充电中")},
                     {QStringLiteral("energyKwh"), energy},
                     {QStringLiteral("powerKw"), power},
                     {QStringLiteral("durationSec"), m_durationSec},
                     {QStringLiteral("feeCents"), feeCents},
                     {QStringLiteral("updatedAt"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))}});
        if (battery >= 80)
            m_tipLabel->setText(QStringLiteral("电量已达到 80%，可按需要结束充电。"));
        if (battery >= 100)
            stopDemoCharging(QStringLiteral("车辆已充满，系统自动停止"));
    });

    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (m_demoMode) startDemoCharging();
        else emit startChargingRequested(m_pile.value(QStringLiteral("reservationId")).toInteger());
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this] {
        if (m_demoMode) stopDemoCharging();
        else emit stopChargingRequested(m_orderId);
    });
    connect(m_faultButton, &QPushButton::clicked, this, [this] {
        if (m_demoMode && m_charging)
            stopDemoCharging(QStringLiteral("检测到设备温度异常，系统已安全停止"));
    });
}

void ChargingPage::setDemoMode(bool enabled)
{
    m_demoMode = enabled;
}

void ChargingPage::setReservation(const QJsonObject &station, const QJsonObject &pile)
{
    m_station = station;
    m_pile = pile;
    m_pile.insert(QStringLiteral("reservationId"), 20260906001LL);
    m_charging = false;
    m_orderId = 0;
    m_snapshot = {};
    m_demoTimer->stop();
    m_stageLabel->setText(QStringLiteral("预约成功 · 15分钟内有效"));
    m_stageLabel->setStyleSheet(QStringLiteral(
        "padding:10px;background:#dcfce7;color:#166534;border-radius:8px;font-weight:600;"));
    m_orderLabel->setText(QStringLiteral("%1\n电桩 %2 · %3 kW\n预约编号：R20260906001")
                              .arg(station.value(QStringLiteral("name")).toString(),
                                   pile.value(QStringLiteral("pileCode")).toString())
                              .arg(pile.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 0));
    m_metricsLabel->setText(QStringLiteral(
        "电量 0.00 kWh\n实时功率 0.0 kW\n已充时长 00:00\n当前费用 ¥0.00"));
    m_batteryProgress->setValue(62);
    m_tipLabel->setText(QStringLiteral("充电前检查通过：预约有效、余额充足、电桩空闲。"));
    m_startButton->setText(QStringLiteral("开始充电"));
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_faultButton->setEnabled(false);
}

bool ChargingPage::hasActiveOrder() const
{
    return m_charging;
}

void ChargingPage::startDemoCharging()
{
    if (m_pile.isEmpty() || m_charging) return;
    m_orderId = 20260906001LL;
    m_durationSec = 126;
    m_charging = true;
    m_stageLabel->setText(QStringLiteral("充电中 · 数据每秒更新"));
    m_stageLabel->setStyleSheet(QStringLiteral(
        "padding:10px;background:#dbeafe;color:#1d4ed8;border-radius:8px;font-weight:600;"));
    m_orderLabel->setText(QStringLiteral("订单 C20260906001\n%1 · %2")
                              .arg(m_station.value(QStringLiteral("name")).toString(),
                                   m_pile.value(QStringLiteral("pileCode")).toString()));
    m_startButton->setEnabled(false);
    m_startButton->setText(QStringLiteral("充电已启动"));
    m_stopButton->setEnabled(true);
    m_faultButton->setEnabled(m_demoMode);
    m_tipLabel->setText(QStringLiteral("连接正常 · 最近一次数据刚刚更新"));
    m_demoTimer->start();
}

void ChargingPage::stopDemoCharging(const QString &reason)
{
    if (!m_charging) return;
    m_demoTimer->stop();
    m_charging = false;
    const double energy = m_snapshot.value(QStringLiteral("energyKwh")).toDouble();
    const double fee = m_snapshot.value(QStringLiteral("feeCents")).toInt() / 100.0;
    m_stageLabel->setText(QStringLiteral("已完成 · 订单结算成功"));
    m_stageLabel->setStyleSheet(QStringLiteral(
        "padding:10px;background:#dcfce7;color:#166534;border-radius:8px;font-weight:600;"));
    m_orderLabel->setText(QStringLiteral(
        "订单 C20260906001\n%1\n本次充电 %2 kWh · 实付 ¥%3")
                              .arg(reason)
                              .arg(energy, 0, 'f', 2)
                              .arg(fee, 0, 'f', 2));
    m_tipLabel->setText(QStringLiteral("费用已从钱包扣除，订单和计费记录已生成（演示数据）。"));
    m_startButton->setEnabled(false);
    m_startButton->setText(QStringLiteral("本次充电已结束"));
    m_stopButton->setEnabled(false);
    m_faultButton->setEnabled(false);
}

void ChargingPage::setSnapshot(const QJsonObject &snapshot)
{
    m_snapshot = snapshot;
    m_lastSnapshotMs = QDateTime::currentMSecsSinceEpoch();
    m_disconnected = false;
    renderSnapshot();
}

void ChargingPage::setDisconnected(bool disconnected)
{
    m_disconnected = disconnected;
    refreshExpiryState();
}

void ChargingPage::renderSnapshot()
{
    if (m_snapshot.isEmpty()) return;
    const int seconds = m_snapshot.value(QStringLiteral("durationSec")).toInt();
    m_metricsLabel->setText(
        QStringLiteral("电量 %1 kWh\n实时功率 %2 kW\n已充时长 %3:%4\n当前费用 ¥%5")
            .arg(m_snapshot.value(QStringLiteral("energyKwh")).toDouble(), 0, 'f', 2)
            .arg(m_snapshot.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 1)
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'))
            .arg(m_snapshot.value(QStringLiteral("feeCents")).toInt() / 100.0, 0, 'f', 2));
}

void ChargingPage::refreshExpiryState()
{
    if (m_snapshot.isEmpty() || m_demoMode) return;
    renderSnapshot();
    const bool expired = m_lastSnapshotMs == 0
        || QDateTime::currentMSecsSinceEpoch() - m_lastSnapshotMs > 10000;
    if (expired)
        m_tipLabel->setText(QStringLiteral("数据已过期：超过10秒未收到服务端快照。"));
    if (m_disconnected)
        m_tipLabel->setText(QStringLiteral("网络已断开，页面展示最后一次有效数据（非实时）。"));
}
