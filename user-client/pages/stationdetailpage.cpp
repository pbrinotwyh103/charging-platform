#include "pages/stationdetailpage.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {

bool pileCanBeReserved(const QJsonObject &pile)
{
    QString status = pile.value(QStringLiteral("status")).toString();
    if (status.trimmed().isEmpty())
        status = pile.value(QStringLiteral("state")).toString();
    status = status.trimmed().toLower();
    return status == QStringLiteral("idle")
        || status == QStringLiteral("available");
}

QString pileStatusText(const QString &status)
{
    const QString normalized = status.toLower();
    if (normalized == QStringLiteral("idle")
        || normalized == QStringLiteral("available"))
        return QStringLiteral("空闲");
    if (normalized == QStringLiteral("reserved")) return QStringLiteral("已预约");
    if (normalized == QStringLiteral("charging")) return QStringLiteral("充电中");
    if (normalized == QStringLiteral("fault")) return QStringLiteral("故障");
    if (normalized == QStringLiteral("offline")) return QStringLiteral("离线");
    return status.isEmpty() ? QStringLiteral("未知") : status;
}

QString pileFieldString(const QJsonObject &pile,
                        const QStringList &keys,
                        const QString &fallback = {})
{
    for (const QString &key : keys) {
        const QJsonValue value = pile.value(key);
        if (value.isString() && !value.toString().trimmed().isEmpty())
            return value.toString().trimmed();
        if (value.isDouble()) return QString::number(value.toVariant().toLongLong());
    }
    return fallback;
}

double pileFieldNumber(const QJsonObject &pile, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = pile.value(key);
        if (value.isDouble()) return value.toDouble();
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        if (ok) return number;
    }
    return 0.0;
}

} // namespace

StationDetailPage::StationDetailPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *backButton = new QPushButton(QStringLiteral("返回附近站点"), this);
    layout->addWidget(backButton);

    m_title = new QLabel(this);
    QFont titleFont = m_title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    m_favorite = new QPushButton(this);
    m_favorite->setObjectName(QStringLiteral("stationFavoriteButton"));
    m_favorite->setMinimumHeight(40);
    layout->addWidget(m_favorite);
    auto *stationActions = new QHBoxLayout;
    m_navigate = new QPushButton(QStringLiteral("路线规划"), this);
    m_navigate->setObjectName(QStringLiteral("stationNavigationButton"));
    m_navigate->setMinimumHeight(38);
    m_navigate->setToolTip(QStringLiteral("使用腾讯地图规划驾车或步行路线"));
    m_refreshPiles = new QPushButton(QStringLiteral("刷新电桩"), this);
    m_refreshPiles->setObjectName(QStringLiteral("refreshPilesButton"));
    stationActions->addWidget(m_navigate, 1);
    stationActions->addWidget(m_refreshPiles, 1);
    layout->addLayout(stationActions);
    m_favoriteStatus = new QLabel(this);
    m_favoriteStatus->setWordWrap(true);
    m_favoriteStatus->setStyleSheet(QStringLiteral("color:#64748b;"));
    layout->addWidget(m_favoriteStatus);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("stationPileStatusLabel"));
    layout->addWidget(m_status);
    m_piles = new QListWidget(this);
    layout->addWidget(m_piles, 1);

    auto *reservationRow = new QHBoxLayout;
    auto *durationLabel = new QLabel(QStringLiteral("保留时长"), this);
    m_duration = new QSpinBox(this);
    m_duration->setObjectName(QStringLiteral("reservationDuration"));
    m_duration->setRange(5, 30);
    m_duration->setValue(15);
    m_duration->setSuffix(QStringLiteral(" 分钟"));
    m_reserve = new QPushButton(QStringLiteral("预约所选电桩"), this);
    m_reserve->setObjectName(QStringLiteral("reservePileButton"));
    m_reserve->setMinimumHeight(42);
    m_reserve->setStyleSheet(QStringLiteral(
        "QPushButton{background:#2563eb;color:white;border:0;border-radius:9px;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"));
    reservationRow->addWidget(durationLabel);
    reservationRow->addWidget(m_duration);
    reservationRow->addWidget(m_reserve, 1);
    layout->addLayout(reservationRow);

    connect(backButton, &QPushButton::clicked,
            this, &StationDetailPage::backRequested);
    connect(m_navigate, &QPushButton::clicked, this, [this] {
        emit navigationRequested(m_station);
    });
    connect(m_refreshPiles, &QPushButton::clicked, this, [this] {
        const qint64 stationId = m_station.value(QStringLiteral("stationId"))
                                     .toVariant().toLongLong();
        if (stationId <= 0) return;
        m_status->setText(QStringLiteral("正在刷新电桩信息…"));
        m_refreshPiles->setDisabled(true);
        emit pilesRequested(stationId);
    });
    connect(m_favorite, &QPushButton::clicked, this, [this] {
        if (m_favoritePending) return;
        const qint64 stationId =
            m_station.value(QStringLiteral("stationId")).toVariant().toLongLong();
        if (stationId <= 0) {
            m_favoriteStatus->setText(QStringLiteral("充电站编号无效，无法收藏"));
            return;
        }
        m_previousFavorite =
            m_station.value(QStringLiteral("favorited")).toBool();
        const bool desired = !m_previousFavorite;
        m_station.insert(QStringLiteral("favorited"), desired);
        m_favoritePending = true;
        m_favoriteStatus->setText(QStringLiteral("正在更新收藏状态…"));
        updateFavoriteButton();
        emit favoriteRequested(stationId, desired);
    });
    connect(m_piles, &QListWidget::currentItemChanged,
            this, [this] { updateReservationButton(); });
    connect(m_reserve, &QPushButton::clicked, this, [this] {
        if (m_reservationPending || !m_piles->currentItem()) return;
        const QJsonObject pile =
            m_piles->currentItem()->data(Qt::UserRole).toJsonObject();
        const qint64 stationId = m_station.value(QStringLiteral("stationId"))
                                     .toVariant().toLongLong();
        const qint64 pileId = pile.value(QStringLiteral("pileId"))
                                  .toVariant().toLongLong();
        if (stationId <= 0 || pileId <= 0
            || !pileCanBeReserved(pile)) {
            m_status->setText(QStringLiteral("请选择一个空闲电桩"));
            return;
        }
        m_reservationPending = true;
        m_status->setText(QStringLiteral("正在创建预约…"));
        updateReservationButton();
        emit reservationRequested(stationId, pileId, m_duration->value());
    });
}

void StationDetailPage::setStation(const QJsonObject &station)
{
    m_station = station;
    m_favoritePending = false;
    m_reservationPending = false;
    m_piles->clear();
    m_favoriteStatus->clear();
    m_title->setText(station.value(QStringLiteral("name")).toString());
    const QJsonValue priceValue = station.contains(QStringLiteral("priceCentsPerKwh"))
        ? station.value(QStringLiteral("priceCentsPerKwh"))
        : station.value(QStringLiteral("price"));
    const double price = station.contains(QStringLiteral("priceCentsPerKwh"))
        ? priceValue.toDouble() / 100.0 : priceValue.toDouble();
    const QJsonValue availableValue = station.contains(QStringLiteral("availablePiles"))
        ? station.value(QStringLiteral("availablePiles"))
        : station.value(QStringLiteral("available"));
    const QJsonValue totalValue = station.contains(QStringLiteral("totalPiles"))
        ? station.value(QStringLiteral("totalPiles"))
        : station.value(QStringLiteral("total"));
    m_summary->setText(QStringLiteral("%1\n电价 ¥%2/度 · 空闲 %3/%4")
        .arg(station.value(QStringLiteral("address")).toString())
        .arg(price, 0, 'f', 2)
        .arg(availableValue.toInt())
        .arg(totalValue.toInt()));
    updateFavoriteButton();
    updateReservationButton();
    m_status->setText(QStringLiteral("正在加载电桩信息…"));
    m_refreshPiles->setDisabled(true);
    m_navigate->setDisabled(!m_station.contains(QStringLiteral("latitude"))
                           || !m_station.contains(QStringLiteral("longitude")));
    emit pilesRequested(
        station.value(QStringLiteral("stationId")).toVariant().toLongLong());
}

void StationDetailPage::setPiles(const QJsonArray &piles)
{
    m_refreshPiles->setDisabled(false);
    m_piles->clear();
    if (piles.isEmpty()) {
        m_status->setText(QStringLiteral("暂无电桩数据"));
        updateReservationButton();
        return;
    }
    QListWidgetItem *firstIdle = nullptr;
    for (const QJsonValue &value : piles) {
        const QJsonObject pile = value.toObject();
        const QString code = pileFieldString(
            pile, {QStringLiteral("pileCode"), QStringLiteral("pileNo"),
                   QStringLiteral("code")}, QStringLiteral("编号未知"));
        const QString type = pileFieldString(
            pile, {QStringLiteral("type"), QStringLiteral("pileType")},
            QStringLiteral("类型未知"));
        const QString status = pileFieldString(
            pile, {QStringLiteral("status"), QStringLiteral("state")},
            QStringLiteral("unknown"));
        const double power = pileFieldNumber(
            pile, {QStringLiteral("powerKw"), QStringLiteral("power")});
        const QString availableAt = pileFieldString(
            pile, {QStringLiteral("availableAt"), QStringLiteral("estimatedAvailableAt"),
                   QStringLiteral("availableTime")});
        QString detail = QStringLiteral("%1 · %2 · %3 kW\n状态：%4")
            .arg(code, type)
            .arg(power, 0, 'f', 1)
            .arg(pileStatusText(status));
        if (!availableAt.isEmpty())
            detail += QStringLiteral("\n预计可用：%1").arg(availableAt);
        auto *item = new QListWidgetItem(
            detail,
            m_piles);
        item->setData(Qt::UserRole, pile);
        if (!pileCanBeReserved(pile))
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable
                           & ~Qt::ItemIsEnabled);
        else if (!firstIdle)
            firstIdle = item;
    }
    if (firstIdle) m_piles->setCurrentItem(firstIdle);
    m_status->setText(QStringLiteral("共 %1 个电桩").arg(piles.size()));
    updateReservationButton();
}

void StationDetailPage::showError(const QString &message)
{
    m_refreshPiles->setDisabled(false);
    m_status->setText(message);
    m_piles->clear();
    updateReservationButton();
}

void StationDetailPage::reservationAccepted()
{
    m_reservationPending = false;
    m_status->setText(QStringLiteral("预约成功，可在充电页开始充电"));
    updateReservationButton();
}

void StationDetailPage::reservationFailed(const QString &message)
{
    m_reservationPending = false;
    m_status->setText(message.isEmpty()
        ? QStringLiteral("预约失败，请稍后重试") : message);
    updateReservationButton();
}

void StationDetailPage::favoriteUpdated(qint64 stationId, bool favorited,
                                        const QString &updatedAt)
{
    if (stationId != m_station.value(QStringLiteral("stationId"))
                         .toVariant().toLongLong()) return;
    m_station.insert(QStringLiteral("favorited"), favorited);
    m_favoritePending = false;
    updateFavoriteButton();
    m_favoriteStatus->setText(
        favorited ? QStringLiteral("已收藏该充电站")
                  : QStringLiteral("已取消收藏"));
    m_favoriteStatus->setToolTip(updatedAt);
    emit favoriteStateChanged(stationId, favorited);
}

void StationDetailPage::favoriteUpdateFailed(qint64 stationId,
                                             const QString &message)
{
    if (stationId != m_station.value(QStringLiteral("stationId"))
                         .toVariant().toLongLong()) return;
    m_station.insert(QStringLiteral("favorited"), m_previousFavorite);
    m_favoritePending = false;
    updateFavoriteButton();
    m_favoriteStatus->setText(
        message.isEmpty() ? QStringLiteral("收藏状态更新失败，已恢复原状态")
                          : message);
}

void StationDetailPage::updateFavoriteButton()
{
    const bool favorited =
        m_station.value(QStringLiteral("favorited")).toBool();
    m_favorite->setDisabled(m_favoritePending);
    if (m_favoritePending) {
        m_favorite->setText(favorited ? QStringLiteral("正在收藏…")
                                      : QStringLiteral("正在取消收藏…"));
    } else {
        m_favorite->setText(favorited ? QStringLiteral("★ 取消收藏")
                                      : QStringLiteral("☆ 收藏站点"));
    }
    m_favorite->setStyleSheet(favorited
        ? QStringLiteral("background:#fef3c7;color:#92400e;border:1px solid #fde68a;border-radius:9px;font-weight:600;")
        : QStringLiteral("background:#eff6ff;color:#1d4ed8;border:1px solid #bfdbfe;border-radius:9px;font-weight:600;"));
}

void StationDetailPage::updateReservationButton()
{
    const bool hasSelection = m_piles && m_piles->currentItem()
        && pileCanBeReserved(
            m_piles->currentItem()->data(Qt::UserRole).toJsonObject());
    m_reserve->setDisabled(m_reservationPending || !hasSelection);
    m_duration->setDisabled(m_reservationPending);
    m_reserve->setText(m_reservationPending
        ? QStringLiteral("正在预约…") : QStringLiteral("预约所选电桩"));
}
