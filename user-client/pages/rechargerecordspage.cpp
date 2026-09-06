#include "pages/rechargerecordspage.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>

namespace {

qint64 integerField(const QJsonObject &object, const QString &primary,
                    const QString &compatibility = QString())
{
    QJsonValue value = object.value(primary);
    if (value.isUndefined() && !compatibility.isEmpty())
        value = object.value(compatibility);
    return value.toVariant().toLongLong();
}

bool hasIntegerField(const QJsonObject &object, const QString &primary,
                     const QString &compatibility = QString())
{
    const QJsonValue value = object.contains(primary)
        ? object.value(primary)
        : object.value(compatibility);
    bool ok = false;
    value.toVariant().toLongLong(&ok);
    return ok;
}

QString moneyText(qint64 fen)
{
    const bool negative = fen < 0;
    const quint64 absolute = negative
        ? static_cast<quint64>(-(fen + 1)) + 1
        : static_cast<quint64>(fen);
    return QStringLiteral("%1¥%2.%3")
        .arg(negative ? QStringLiteral("-") : QString())
        .arg(absolute / 100)
        .arg(absolute % 100, 2, 10, QLatin1Char('0'));
}

qint64 createdAtMillis(const QJsonObject &record)
{
    const QJsonValue value = record.value(QStringLiteral("createdAt"));
    if (value.isDouble())
        return value.toVariant().toLongLong();

    const QDateTime dateTime = QDateTime::fromString(value.toString(), Qt::ISODate);
    return dateTime.isValid() ? dateTime.toMSecsSinceEpoch() : 0;
}

QString createdAtText(const QJsonObject &record)
{
    const qint64 milliseconds = createdAtMillis(record);
    if (milliseconds <= 0)
        return QStringLiteral("时间未知");
    return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::utc())
        .toTimeZone(QTimeZone("Asia/Shanghai"))
        .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString stateText(const QString &state)
{
    const QString normalized = state.trimmed().toUpper();
    if (normalized == QStringLiteral("SUCCESS")) return QStringLiteral("支付成功");
    if (normalized == QStringLiteral("FAILED")) return QStringLiteral("支付失败");
    if (normalized == QStringLiteral("PENDING")) return QStringLiteral("处理中");
    return QStringLiteral("状态未知");
}

QString stateStyle(const QString &state)
{
    const QString normalized = state.trimmed().toUpper();
    if (normalized == QStringLiteral("SUCCESS"))
        return QStringLiteral("padding:4px 9px;background:#dcfce7;color:#166534;border-radius:9px;font-weight:600;");
    if (normalized == QStringLiteral("FAILED"))
        return QStringLiteral("padding:4px 9px;background:#fee2e2;color:#991b1b;border-radius:9px;font-weight:600;");
    return QStringLiteral("padding:4px 9px;background:#fef3c7;color:#92400e;border-radius:9px;font-weight:600;");
}

} // namespace

RechargeRecordsPage::RechargeRecordsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("rechargeRecordsPage"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *backButton = new QPushButton(QStringLiteral("返回"), this);
    backButton->setObjectName(QStringLiteral("rechargeRecordsBackButton"));
    backButton->setFlat(true);
    backButton->setStyleSheet(QStringLiteral("color:#2563eb;font-weight:600;padding:6px;"));

    auto *titleLabel = new QLabel(QStringLiteral("充值记录"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    m_refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshButton->setObjectName(QStringLiteral("rechargeRecordsRefreshButton"));
    m_refreshButton->setFlat(true);
    m_refreshButton->setStyleSheet(QStringLiteral("color:#2563eb;font-weight:600;padding:6px;"));
    header->addWidget(backButton);
    header->addStretch();
    header->addWidget(titleLabel);
    header->addStretch();
    header->addWidget(m_refreshButton);
    layout->addLayout(header);

    m_summaryLabel = new QLabel(QStringLiteral("充值流水以服务端记录为准"), this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet(QStringLiteral(
        "padding:12px;background:#eff6ff;color:#1e3a8a;border-radius:10px;"));
    layout->addWidget(m_summaryLabel);

    m_recordsList = new QListWidget(this);
    m_recordsList->setObjectName(QStringLiteral("rechargeRecordsList"));
    m_recordsList->setFrameShape(QFrame::NoFrame);
    m_recordsList->setSpacing(8);
    m_recordsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_recordsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recordsList->setStyleSheet(QStringLiteral(
        "QListWidget{background:transparent;}"
        "QListWidget::item{background:#ffffff;border:1px solid #e2e8f0;border-radius:10px;}"));
    layout->addWidget(m_recordsList, 1);

    auto *statePanel = new QWidget(this);
    statePanel->setObjectName(QStringLiteral("rechargeRecordsStatePanel"));
    auto *stateLayout = new QVBoxLayout(statePanel);
    stateLayout->setContentsMargins(20, 32, 20, 32);
    stateLayout->setSpacing(8);
    m_stateTitleLabel = new QLabel(statePanel);
    m_stateTitleLabel->setObjectName(QStringLiteral("rechargeRecordsStateTitle"));
    m_stateTitleLabel->setAlignment(Qt::AlignCenter);
    QFont stateTitleFont = m_stateTitleLabel->font();
    stateTitleFont.setPointSize(16);
    stateTitleFont.setWeight(QFont::DemiBold);
    m_stateTitleLabel->setFont(stateTitleFont);
    m_stateTitleLabel->setStyleSheet(QStringLiteral("color:#334155;"));
    m_stateDescriptionLabel = new QLabel(statePanel);
    m_stateDescriptionLabel->setObjectName(QStringLiteral("rechargeRecordsStateDescription"));
    m_stateDescriptionLabel->setAlignment(Qt::AlignCenter);
    m_stateDescriptionLabel->setWordWrap(true);
    m_stateDescriptionLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
    m_retryButton = new QPushButton(QStringLiteral("重新加载"), statePanel);
    m_retryButton->setObjectName(QStringLiteral("rechargeRecordsRetryButton"));
    m_retryButton->setMinimumHeight(40);
    m_retryButton->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"));
    stateLayout->addStretch();
    stateLayout->addWidget(m_stateTitleLabel);
    stateLayout->addWidget(m_stateDescriptionLabel);
    stateLayout->addWidget(m_retryButton);
    stateLayout->addStretch();
    layout->addWidget(statePanel, 1);

    connect(backButton, &QPushButton::clicked, this, &RechargeRecordsPage::backRequested);
    connect(m_refreshButton, &QPushButton::clicked, this, &RechargeRecordsPage::recordsRequested);
    connect(m_retryButton, &QPushButton::clicked, this, &RechargeRecordsPage::recordsRequested);

    showState(QStringLiteral("暂无充值记录"),
              QStringLiteral("完成充值后，记录会显示在这里"), false);
}

void RechargeRecordsPage::showLoading()
{
    m_refreshButton->setDisabled(true);
    m_recordsList->clear();
    showState(QStringLiteral("正在加载充值记录…"),
              QStringLiteral("请稍候"), false);
}

void RechargeRecordsPage::showError(const QString &message)
{
    m_refreshButton->setDisabled(false);
    m_recordsList->clear();
    showState(QStringLiteral("充值记录加载失败"),
              message.isEmpty() ? QStringLiteral("请检查网络后重试") : message,
              true);
}

void RechargeRecordsPage::setRecords(const QJsonArray &records)
{
    m_refreshButton->setDisabled(false);
    m_recordsList->clear();

    QList<QJsonObject> sorted;
    sorted.reserve(records.size());
    for (const QJsonValue &value : records) {
        if (value.isObject()) sorted.append(value.toObject());
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const QJsonObject &left,
                                                       const QJsonObject &right) {
        const qint64 leftTime = createdAtMillis(left);
        const qint64 rightTime = createdAtMillis(right);
        if (leftTime != rightTime) return leftTime > rightTime;
        const QString leftId = left.value(QStringLiteral("rechargeId")).toString();
        const QString rightId = right.value(QStringLiteral("rechargeId")).toString();
        bool leftNumeric = false;
        bool rightNumeric = false;
        const qint64 leftNumber = leftId.toLongLong(&leftNumeric);
        const qint64 rightNumber = rightId.toLongLong(&rightNumeric);
        if (leftNumeric && rightNumeric) return leftNumber > rightNumber;
        return leftId > rightId;
    });

    if (sorted.isEmpty()) {
        showState(QStringLiteral("暂无充值记录"),
                  QStringLiteral("完成充值后，记录会显示在这里"), false);
        m_summaryLabel->setText(QStringLiteral("当前没有充值流水"));
        return;
    }

    findChild<QWidget *>(QStringLiteral("rechargeRecordsStatePanel"))->hide();
    m_recordsList->show();
    m_summaryLabel->setText(QStringLiteral("共 %1 条充值记录 · 已按时间倒序排列")
                                .arg(sorted.size()));
    for (const QJsonObject &record : sorted) renderRecord(record);
}

void RechargeRecordsPage::showState(const QString &title,
                                    const QString &description,
                                    bool retryVisible)
{
    m_recordsList->hide();
    auto *statePanel = findChild<QWidget *>(QStringLiteral("rechargeRecordsStatePanel"));
    statePanel->show();
    m_stateTitleLabel->setText(title);
    m_stateDescriptionLabel->setText(description);
    m_retryButton->setVisible(retryVisible);
}

void RechargeRecordsPage::renderRecord(const QJsonObject &record)
{
    const qint64 amountFen = integerField(record, QStringLiteral("amountFen"),
                                         QStringLiteral("amountCents"));
    const QJsonObject normalized = [&record] {
        QJsonObject result = record;
        if (!result.contains(QStringLiteral("balanceAfterFen"))
            && result.contains(QStringLiteral("balanceAfterCents")))
            result.insert(QStringLiteral("balanceAfterFen"),
                          result.value(QStringLiteral("balanceAfterCents")));
        return result;
    }();
    const qint64 balanceFen = integerField(normalized, QStringLiteral("balanceAfterFen"),
                                          QStringLiteral("balanceCents"));
    const bool hasAmount = hasIntegerField(record, QStringLiteral("amountFen"),
                                           QStringLiteral("amountCents"));
    const bool hasBalance = hasIntegerField(normalized, QStringLiteral("balanceAfterFen"),
                                            QStringLiteral("balanceCents"));
    const QString state = record.value(QStringLiteral("state")).toString(
        record.value(QStringLiteral("status")).toString());

    auto *item = new QListWidgetItem(m_recordsList);
    auto *card = new QWidget(m_recordsList);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(7);

    auto *topRow = new QHBoxLayout;
    auto *amountLabel = new QLabel(
        hasAmount ? QStringLiteral("+ %1").arg(moneyText(amountFen))
                  : QStringLiteral("充值金额未知"),
        card);
    amountLabel->setStyleSheet(QStringLiteral("font-weight:700;color:#0f172a;"));
    auto *stateLabel = new QLabel(stateText(state), card);
    stateLabel->setStyleSheet(stateStyle(state));
    topRow->addWidget(amountLabel);
    topRow->addStretch();
    topRow->addWidget(stateLabel);

    auto *timeLabel = new QLabel(QStringLiteral("充值时间  %1").arg(createdAtText(record)), card);
    timeLabel->setStyleSheet(QStringLiteral("color:#64748b;"));
    auto *balanceLabel = new QLabel(hasBalance
        ? QStringLiteral("充值后余额  %1").arg(moneyText(balanceFen))
        : QStringLiteral("充值后余额  --"), card);
    balanceLabel->setStyleSheet(QStringLiteral("color:#475569;"));

    cardLayout->addLayout(topRow);
    cardLayout->addWidget(timeLabel);
    cardLayout->addWidget(balanceLabel);
    card->adjustSize();
    item->setSizeHint(QSize(m_recordsList->viewport()->width(), card->sizeHint().height()));
    m_recordsList->setItemWidget(item, card);
}
