#include "pages/stationdetailpage.h"
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QtMath>

namespace {
QString pileTypeText(const QString &type)
{
    if (type == QStringLiteral("fast")) return QStringLiteral("快充");
    if (type == QStringLiteral("slow")) return QStringLiteral("慢充");
    return QStringLiteral("类型未知");
}

QString pileStatusText(const QString &status)
{
    if (status == QStringLiteral("available")) return QStringLiteral("空闲");
    if (status == QStringLiteral("charging")) return QStringLiteral("充电中");
    if (status == QStringLiteral("reserved")) return QStringLiteral("已预约");
    if (status == QStringLiteral("fault")) return QStringLiteral("故障");
    if (status == QStringLiteral("offline")) return QStringLiteral("离线");
    if (status == QStringLiteral("disabled")) return QStringLiteral("已停用");
    return QStringLiteral("未知状态");
}

QString estimatedAvailabilityText(const QJsonObject &pile, bool available)
{
    if (available) return QStringLiteral("可立即预约");
    const QStringList timeKeys{QStringLiteral("estimatedAvailableAt"),
                               QStringLiteral("expectedAvailableAt"),
                               QStringLiteral("availableAt")};
    for (const QString &key : timeKeys) {
        const QString value = pile.value(key).toString().trimmed();
        if (!value.isEmpty()) return QStringLiteral("预计可用：%1").arg(value);
    }
    const QJsonValue minutesValue = pile.value(QStringLiteral("estimatedAvailableMinutes"));
    if (minutesValue.isDouble() && qIsFinite(minutesValue.toDouble())
        && minutesValue.toDouble() >= 0.0)
        return QStringLiteral("预计约 %1 分钟后可用")
            .arg(qCeil(minutesValue.toDouble()));
    return QStringLiteral("预计可用时间：暂未提供");
}

QString stationSummary(const QJsonObject &station)
{
    const QString address = station.value(QStringLiteral("address")).toString().trimmed().isEmpty()
        ? QStringLiteral("地址暂未提供")
        : station.value(QStringLiteral("address")).toString().trimmed();
    QStringList details;
    const QJsonValue priceValue = station.value(QStringLiteral("priceCentsPerKwh"));
    if (priceValue.isDouble() && qIsFinite(priceValue.toDouble()) && priceValue.toDouble() >= 0)
        details << QStringLiteral("电价 ¥%1/度").arg(priceValue.toDouble() / 100.0, 0, 'f', 2);
    else
        details << QStringLiteral("电价未知");
    const QJsonValue availableValue = station.value(QStringLiteral("availablePiles"));
    const QJsonValue totalValue = station.value(QStringLiteral("totalPiles"));
    if (availableValue.isDouble() && totalValue.isDouble()
        && availableValue.toInt(-1) >= 0 && totalValue.toInt(-1) >= availableValue.toInt(-1))
        details << QStringLiteral("空闲 %1/%2").arg(availableValue.toInt()).arg(totalValue.toInt());
    else
        details << QStringLiteral("空闲桩数未知");
    const QJsonValue distanceValue = station.value(QStringLiteral("distanceKm"));
    if (distanceValue.isDouble() && qIsFinite(distanceValue.toDouble()) && distanceValue.toDouble() >= 0)
        details << QStringLiteral("距离 %1 km").arg(distanceValue.toDouble(), 0, 'f', 1);
    else
        details << QStringLiteral("距离未知");
    return address + QStringLiteral("\n") + details.join(QStringLiteral(" · "));
}
}

StationDetailPage::StationDetailPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("stationDetailPage"));
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(16, 12, 16, 12);
    m_rootLayout->setSpacing(10);

    auto *back = new QPushButton(QStringLiteral("← 返回附近站点"), this);
    back->setObjectName(QStringLiteral("stationBackButton"));
    back->setFlat(true);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("stationDetailTitle"));
    QFont titleFont = m_title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);

    m_actionLayout = new QHBoxLayout;
    m_actionLayout->setObjectName(QStringLiteral("stationActionLayout"));
    m_favorite = new QPushButton(this);
    m_favorite->setObjectName(QStringLiteral("favoriteButton"));
    auto *drive = new QPushButton(QStringLiteral("驾车导航"), this);
    drive->setObjectName(QStringLiteral("drivingNavigationButton"));
    auto *walk = new QPushButton(QStringLiteral("步行导航"), this);
    walk->setObjectName(QStringLiteral("walkingNavigationButton"));
    m_actionLayout->addWidget(m_favorite);
    m_actionLayout->addWidget(drive);
    m_actionLayout->addWidget(walk);

    auto *sectionRow = new QHBoxLayout;
    auto *section = new QLabel(QStringLiteral("选择可用充电桩"), this);
    QFont sectionFont = section->font();
    sectionFont.setBold(true);
    section->setFont(sectionFont);
    m_refresh = new QPushButton(QStringLiteral("刷新状态"), this);
    m_refresh->setObjectName(QStringLiteral("pileRefreshButton"));
    sectionRow->addWidget(section);
    sectionRow->addStretch();
    sectionRow->addWidget(m_refresh);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("stationStatusLabel"));
    m_status->setWordWrap(true);
    m_piles = new QListWidget(this);
    m_piles->setObjectName(QStringLiteral("pileList"));
    m_piles->setSpacing(4);
    m_reserve = new QPushButton(QStringLiteral("请选择空闲电桩"), this);
    m_reserve->setObjectName(QStringLiteral("reserveButton"));
    m_reserve->setEnabled(false);
    m_reserve->setMinimumHeight(42);
    m_rootLayout->addWidget(back);
    m_rootLayout->addWidget(m_title);
    m_rootLayout->addWidget(m_summary);
    m_rootLayout->addLayout(m_actionLayout);
    m_rootLayout->addLayout(sectionRow);
    m_rootLayout->addWidget(m_status);
    m_rootLayout->addWidget(m_piles, 1);
    m_rootLayout->addWidget(m_reserve);

    connect(back, &QPushButton::clicked, this, &StationDetailPage::backRequested);
    connect(m_favorite, &QPushButton::clicked, this, [this] {
        if (!m_favorite->isEnabled()) return;
        const bool next = !m_station.value(QStringLiteral("favorited")).toBool();
        m_favoriteBeforeRequest = !next;
        setFavoriteState(next);
        m_favorite->setEnabled(false);
        m_favorite->setText(next ? QStringLiteral("正在收藏…") : QStringLiteral("正在取消收藏…"));
        emit favoriteRequested(m_station.value(QStringLiteral("stationId")).toInteger(), next);
    });
    connect(drive, &QPushButton::clicked, this, [this] {
        if (stationId() > 0)
            emit navigationRequested(m_station, QStringLiteral("driving"));
    });
    connect(walk, &QPushButton::clicked, this, [this] {
        if (stationId() > 0)
            emit navigationRequested(m_station, QStringLiteral("walking"));
    });
    connect(m_refresh, &QPushButton::clicked, this, [this] {
        if (stationId() <= 0 || !m_refresh->isEnabled()) return;
        m_refresh->setEnabled(false);
        m_refresh->setText(QStringLiteral("正在刷新…"));
        m_status->setText(QStringLiteral("正在加载电桩运行状态…"));
        emit pilesRequested(stationId());
    });
    connect(m_piles, &QListWidget::currentItemChanged, this,
            [this] { updateSelection(); });
    connect(m_reserve, &QPushButton::clicked, this, [this] {
        if (!m_piles->currentItem()) return;
        const QJsonObject pile = m_piles->currentItem()->data(Qt::UserRole).toJsonObject();
        if (pile.value(QStringLiteral("pileId")).toInteger() > 0
            && pile.value(QStringLiteral("status")).toString() == QStringLiteral("available"))
            emit reservationRequested(m_station, pile);
    });
}

void StationDetailPage::setCompactLayout(bool compact)
{
    m_actionLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                         : QBoxLayout::LeftToRight);
    m_actionLayout->setSpacing(compact ? 7 : 10);
    m_rootLayout->setContentsMargins(compact ? 6 : 18, compact ? 6 : 14,
                                     compact ? 6 : 18, compact ? 6 : 14);
}

void StationDetailPage::setStation(const QJsonObject &station)
{
    if (!m_keepDirectPile) m_directPile = {};
    const qint64 stationId = station.value(QStringLiteral("stationId")).toInteger();
    if (stationId <= 0) {
        m_station = {};
        m_title->setText(QStringLiteral("未选择充电站"));
        m_summary->setText(QStringLiteral("请返回附近站点，选择一个有效充电站后再查看电桩和导航。"));
        m_status->setText(QStringLiteral("请先选择有效的充电站"));
        m_piles->clear();
        m_reserve->setEnabled(false);
        m_refresh->setEnabled(false);
        m_refresh->setText(QStringLiteral("刷新状态"));
        m_favoriteBeforeRequest = false;
        m_favorite->setEnabled(false);
        setFavoriteState(false);
        return;
    }
    m_station = station;
    m_favoriteBeforeRequest = station.value(QStringLiteral("favorited")).toBool();
    m_favorite->setEnabled(true);
    const QString stationName = station.value(QStringLiteral("name")).toString().trimmed();
    m_title->setText(stationName.isEmpty() ? QStringLiteral("未命名充电站") : stationName);
    m_summary->setText(stationSummary(station));
    setFavoriteState(station.value(QStringLiteral("favorited")).toBool());
    m_status->setText(QStringLiteral("正在加载电桩运行状态…"));
    m_piles->clear();
    m_reserve->setEnabled(false);
    m_refresh->setEnabled(false);
    m_refresh->setText(QStringLiteral("正在刷新…"));
    emit pilesRequested(stationId);
}

void StationDetailPage::setDirectPile(const QJsonObject &station,
                                      const QJsonObject &pile)
{
    m_directPile = pile;
    m_keepDirectPile = true;
    setStation(station);
    m_keepDirectPile = false;
    const auto *selected = m_piles->currentItem();
    if (!selected || selected->data(Qt::UserRole).toJsonObject()
                         .value(QStringLiteral("pileId")).toInteger()
                     != pile.value(QStringLiteral("pileId")).toInteger())
        m_status->setText(QStringLiteral("已识别电桩 %1，正在读取状态…")
                              .arg(pile.value(QStringLiteral("pileCode")).toString()));
}

void StationDetailPage::clearStation()
{
    setStation({});
}

void StationDetailPage::setPiles(const QJsonArray &piles)
{
    m_piles->clear();
    m_refresh->setEnabled(stationId() > 0);
    m_refresh->setText(QStringLiteral("刷新状态"));
    if (piles.isEmpty() && m_directPile.isEmpty()) {
        m_status->setText(QStringLiteral("该站点暂无电桩数据 · 更新于刚刚"));
        return;
    }
    int directRow = -1;
    const qint64 directId = m_directPile.value(QStringLiteral("pileId")).toInteger();
    for (const auto &value : piles) {
        const QJsonObject pile = value.toObject();
        const bool validPile = pile.value(QStringLiteral("pileId")).toInteger() > 0;
        const bool available = validPile
            && pile.value(QStringLiteral("status")).toString() == QStringLiteral("available");
        const QString code = pile.value(QStringLiteral("pileCode")).toString().trimmed();
        const QJsonValue powerValue = pile.value(QStringLiteral("powerKw"));
        const QString power = powerValue.isDouble() && qIsFinite(powerValue.toDouble())
            && powerValue.toDouble() > 0
            ? QStringLiteral("%1 kW").arg(powerValue.toDouble(), 0, 'f', 1)
            : QStringLiteral("功率未知");
        const QString updatedAt = pile.value(QStringLiteral("updatedAt")).toString().trimmed();
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  ·  %2  ·  %3\n%4 · %5%6")
                .arg(code.isEmpty() ? QStringLiteral("未命名电桩") : code,
                     pileTypeText(pile.value(QStringLiteral("type")).toString()), power,
                     validPile ? pileStatusText(pile.value(QStringLiteral("status")).toString())
                               : QStringLiteral("数据异常（缺少电桩标识）"),
                     estimatedAvailabilityText(pile, available),
                     updatedAt.isEmpty() ? QString() : QStringLiteral(" · 状态时间 %1").arg(updatedAt)),
            m_piles);
        item->setData(Qt::UserRole, QVariant::fromValue(pile));
        if (!available)
            item->setForeground(QColor(QStringLiteral("#94a3b8")));
        if (pile.value(QStringLiteral("pileId")).toInteger() == directId)
            directRow = m_piles->count() - 1;
    }
    if (directId > 0 && directRow < 0) {
        QJsonArray merged = piles;
        merged.append(m_directPile);
        setPiles(merged);
        return;
    }
    if (directRow >= 0) {
        m_piles->setCurrentRow(directRow);
        const QString code = m_piles->item(directRow)->data(Qt::UserRole).toJsonObject()
                                 .value(QStringLiteral("pileCode")).toString();
        m_status->setText(QStringLiteral("已通过编号连接到 %1").arg(code));
        return;
    }
    m_status->setText(m_demoMode
        ? QStringLiteral("共 %1 个电桩 · 运行状态为演示模拟数据，非设备实时状态").arg(piles.size())
        : QStringLiteral("共 %1 个电桩 · 服务端状态更新于刚刚").arg(piles.size()));
}

void StationDetailPage::showError(const QString &message)
{
    m_status->setText(message);
    m_piles->clear();
    m_reserve->setEnabled(false);
    m_refresh->setEnabled(stationId() > 0);
    m_refresh->setText(QStringLiteral("重新刷新"));
    auto *item = new QListWidgetItem(QStringLiteral("暂时无法获取电桩状态"), m_piles);
    item->setFlags(Qt::NoItemFlags);
}

void StationDetailPage::setFavoriteState(bool favorited)
{
    m_station.insert(QStringLiteral("favorited"), favorited);
    m_favorite->setText(favorited ? QStringLiteral("★ 已收藏")
                                  : QStringLiteral("☆ 收藏站点"));
}

void StationDetailPage::favoriteUpdateSucceeded(bool favorited)
{
    setFavoriteState(favorited);
    m_favorite->setEnabled(true);
}

void StationDetailPage::favoriteUpdateFailed(const QString &message)
{
    setFavoriteState(m_favoriteBeforeRequest);
    m_favorite->setEnabled(true);
    m_status->setText(QStringLiteral("收藏操作失败：%1").arg(message));
}

void StationDetailPage::updateSelection()
{
    const auto *item = m_piles->currentItem();
    const QJsonObject pile = item ? item->data(Qt::UserRole).toJsonObject() : QJsonObject();
    const bool available = item && pile.value(QStringLiteral("pileId")).toInteger() > 0
        && pile.value(QStringLiteral("status")).toString() == QStringLiteral("available");
    m_reserve->setEnabled(available);
    m_reserve->setText(available ? QStringLiteral("预约此充电桩")
                                 : QStringLiteral("请选择空闲电桩"));
}
