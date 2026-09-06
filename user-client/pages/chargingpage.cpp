#include "pages/chargingpage.h"

#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
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
            "当前暂无进行中的充电任务\n\n"
            "开始充电后，这里将实时展示：\n\n"
            "• 预约与充电状态\n"
            "• 累计电量和实时功率\n"
            "• 充电时长和当前费用\n"
            "• 最后更新时间"),
        this);

    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(16);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#1e293b;"));

    auto *statusCard = new QFrame(this);
    statusCard->setObjectName(QStringLiteral("chargingStatusCard"));
    statusCard->setStyleSheet(QStringLiteral(
        "QFrame#chargingStatusCard{background:#f8fafc;border:1px solid #dbe4f0;border-radius:14px;}"));
    auto *statusCardLayout = new QVBoxLayout(statusCard);
    statusCardLayout->setContentsMargins(24, 24, 24, 24);
    statusCardLayout->addWidget(m_statusLabel, 1);

    m_actionStatusLabel = new QLabel(this);
    m_actionStatusLabel->setObjectName(QStringLiteral("chargingActionStatus"));
    m_actionStatusLabel->setWordWrap(true);
    m_actionStatusLabel->setStyleSheet(QStringLiteral("color:#b45309;"));
    m_actionStatusLabel->hide();

    auto *actions = new QHBoxLayout;
    m_startButton = new QPushButton(QStringLiteral("开始充电"), this);
    m_startButton->setObjectName(QStringLiteral("startChargingButton"));
    m_cancelButton = new QPushButton(QStringLiteral("取消预约"), this);
    m_cancelButton->setObjectName(QStringLiteral("cancelReservationButton"));
    m_stopButton = new QPushButton(QStringLiteral("停止充电"), this);
    m_stopButton->setObjectName(QStringLiteral("stopChargingButton"));
    for (QPushButton *button : {m_startButton, m_cancelButton, m_stopButton}) {
        button->setMinimumHeight(42);
        actions->addWidget(button);
    }
    m_startButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#16a34a;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"));
    m_cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#f1f5f9;color:#475569;border:1px solid #cbd5e1;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{color:#94a3b8;}"));
    m_stopButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#dc2626;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"));

    layout->addWidget(m_titleLabel);
    layout->addWidget(statusCard, 1);
    layout->addWidget(m_actionStatusLabel);
    layout->addLayout(actions);

    const auto beginAction = [this](QPushButton *source) {
        m_actionPending = true;
        m_actionStatusLabel->setText(
            QStringLiteral("%1请求已发送，请稍候…").arg(source->text()));
        m_actionStatusLabel->show();
        updateActions();
    };
    connect(m_startButton, &QPushButton::clicked, this, [this, beginAction] {
        const qint64 reservationId =
            m_reservation.value(QStringLiteral("reservationId"))
                .toVariant().toLongLong();
        if (reservationId <= 0) return;
        beginAction(m_startButton);
        emit startRequested(reservationId);
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this, beginAction] {
        const qint64 reservationId =
            m_reservation.value(QStringLiteral("reservationId"))
                .toVariant().toLongLong();
        if (reservationId <= 0) return;
        beginAction(m_cancelButton);
        emit cancelRequested(reservationId);
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this, beginAction] {
        const qint64 orderId = m_snapshot.value(QStringLiteral("orderId"))
                                   .toVariant().toLongLong();
        if (orderId <= 0) return;
        beginAction(m_stopButton);
        emit stopRequested(orderId);
    });

    m_expiryTimer = new QTimer(this);
    m_expiryTimer->setInterval(1000);
    connect(m_expiryTimer, &QTimer::timeout, this, &ChargingPage::refreshExpiryState);
    m_expiryTimer->start();
    updateActions();
}
void ChargingPage::setSnapshot(const QJsonObject &s)
{
    m_snapshot = s;
    m_reservation = {};
    m_lastSnapshotMs = QDateTime::currentMSecsSinceEpoch();
    m_disconnected = false;
    m_actionPending = false;
    m_actionStatusLabel->hide();
    refreshExpiryState();
    updateActions();
}

void ChargingPage::setReservation(const QJsonObject &reservation)
{
    m_reservation = reservation;
    m_snapshot = {};
    m_lastSnapshotMs = 0;
    m_actionPending = false;
    m_disconnected = false;
    m_actionStatusLabel->hide();
    refreshExpiryState();
    updateActions();
}

void ChargingPage::setNoActiveTask()
{
    m_reservation = {};
    m_snapshot = {};
    m_lastSnapshotMs = 0;
    m_actionPending = false;
    m_actionStatusLabel->hide();
    m_statusLabel->setText(QStringLiteral(
        "当前暂无进行中的充电任务\n\n"
        "请先在首页选择充电站和空闲电桩，完成预约后再开始充电。"));
    updateActions();
}
void ChargingPage::setDisconnected(bool disconnected)
{
    m_disconnected = disconnected;
    refreshExpiryState();
    updateActions();
}

void ChargingPage::showActionError(const QString &message)
{
    m_actionPending = false;
    m_actionStatusLabel->setText(message.isEmpty()
        ? QStringLiteral("操作失败，请稍后重试") : message);
    m_actionStatusLabel->show();
    updateActions();
}

void ChargingPage::refreshExpiryState()
{
    if (m_snapshot.isEmpty()) {
        if (m_reservation.isEmpty()) return;
        const QDateTime expiresAt = QDateTime::fromString(
            m_reservation.value(QStringLiteral("expiresAt")).toString(),
            Qt::ISODate);
        const qint64 remaining = expiresAt.isValid()
            ? QDateTime::currentDateTimeUtc().secsTo(expiresAt.toUTC()) : -1;
        QString expiryText = m_reservation.value(QStringLiteral("expiresAt"))
                                 .toString();
        if (remaining >= 0)
            expiryText = QStringLiteral("%1（剩余 %2 分 %3 秒）")
                .arg(expiresAt.toLocalTime().toString(
                         QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                .arg(remaining / 60).arg(remaining % 60);
        m_statusLabel->setText(QStringLiteral(
            "预约成功\n站点编号：%1\n电桩编号：%2\n预约有效期：%3\n\n"
            "到达充电桩后，点击“开始充电”。")
            .arg(m_reservation.value(QStringLiteral("stationId"))
                     .toVariant().toString(),
                 m_reservation.value(QStringLiteral("pileId"))
                     .toVariant().toString(),
                 expiryText));
        if (remaining < 0 && expiresAt.isValid())
            m_statusLabel->setText(m_statusLabel->text()
                + QStringLiteral("\n预约已过期，请重新预约。"));
        updateActions();
        return;
    }
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

void ChargingPage::updateActions()
{
    const bool hasReservation = !m_reservation.isEmpty();
    const QString status = m_snapshot.value(QStringLiteral("status"))
                               .toString().toLower();
    const bool charging = status == QStringLiteral("charging")
        || status == QStringLiteral("stopping")
        || status.contains(QStringLiteral("充电中"));
    bool reservationExpired = false;
    if (hasReservation) {
        const QDateTime expiresAt = QDateTime::fromString(
            m_reservation.value(QStringLiteral("expiresAt")).toString(),
            Qt::ISODate);
        reservationExpired = expiresAt.isValid()
            && expiresAt.toUTC() <= QDateTime::currentDateTimeUtc();
    }
    const bool enabled = !m_disconnected && !m_actionPending;
    m_startButton->setVisible(hasReservation);
    m_cancelButton->setVisible(hasReservation);
    m_stopButton->setVisible(charging);
    m_startButton->setEnabled(enabled && hasReservation && !reservationExpired);
    m_cancelButton->setEnabled(enabled && hasReservation && !reservationExpired);
    m_stopButton->setEnabled(enabled && charging
                             && status != QStringLiteral("stopping"));
}
