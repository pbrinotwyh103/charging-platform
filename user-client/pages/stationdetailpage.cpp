#include "pages/stationdetailpage.h"
#include "map/mapnavigator.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSizePolicy>
#include <QVariant>
#include <QVBoxLayout>
#include <QWebEngineView>

namespace {
QString pileTypeText(const QString &type)
{
    return type == QStringLiteral("fast") ? QStringLiteral("快充")
                                           : QStringLiteral("慢充");
}

QString pileStatusText(const QString &status)
{
    if (status == QStringLiteral("available")) return QStringLiteral("空闲");
    if (status == QStringLiteral("charging")) return QStringLiteral("充电中");
    if (status == QStringLiteral("reserved")) return QStringLiteral("已预约");
    if (status == QStringLiteral("fault")) return QStringLiteral("故障");
    if (status == QStringLiteral("offline")) return QStringLiteral("离线");
    return status;
}

QListWidgetItem *addPileItem(QListWidget *list, const QJsonObject &pile)
{
    const bool available = pile.value(QStringLiteral("status")).toString()
                           == QStringLiteral("available");
    auto *item = new QListWidgetItem(
        QStringLiteral("%1  ·  %2  ·  %3 kW\n%4%5")
            .arg(pile.value(QStringLiteral("pileCode")).toString(),
                 pileTypeText(pile.value(QStringLiteral("type")).toString()))
            .arg(pile.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 0)
            .arg(pileStatusText(pile.value(QStringLiteral("status")).toString()),
                 available ? QStringLiteral(" · 可立即预约") : QString()),
        list);
    item->setData(Qt::UserRole, QVariant::fromValue(pile));
    if (!available) item->setForeground(QColor(QStringLiteral("#94a3b8")));
    return item;
}
}

StationDetailPage::StationDetailPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("stationDetailPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

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

    auto *actions = new QHBoxLayout;
    m_favorite = new QPushButton(this);
    auto *drive = new QPushButton(QStringLiteral("驾车导航"), this);
    drive->setObjectName(QStringLiteral("driveNavigationButton"));
    auto *walk = new QPushButton(QStringLiteral("步行导航"), this);
    walk->setObjectName(QStringLiteral("walkNavigationButton"));
    actions->addWidget(m_favorite);
    actions->addWidget(drive);
    actions->addWidget(walk);

    m_pileSection = new QLabel(QStringLiteral("选择可用充电桩"), this);
    QFont sectionFont = m_pileSection->font();
    sectionFont.setBold(true);
    m_pileSection->setFont(sectionFont);
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

    m_navigationBar = new QWidget(this);
    m_navigationBar->setObjectName(QStringLiteral("navigationBar"));
    auto *navigationLayout = new QHBoxLayout(m_navigationBar);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    m_navigationTitle = new QLabel(this);
    m_navigationTitle->setObjectName(QStringLiteral("navigationTitle"));
    m_navigationTitle->setWordWrap(true);
    QFont navigationFont = m_navigationTitle->font();
    navigationFont.setBold(true);
    m_navigationTitle->setFont(navigationFont);
    auto *closeNavigation = new QPushButton(QStringLiteral("返回电桩列表"), this);
    closeNavigation->setObjectName(QStringLiteral("closeNavigationButton"));
    navigationLayout->addWidget(m_navigationTitle, 1);
    navigationLayout->addWidget(closeNavigation);
    m_navigationBar->hide();

    m_mapView = new QWebEngineView(this);
    m_mapView->setObjectName(QStringLiteral("navigationWebView"));
    m_mapView->setMinimumHeight(420);
    m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mapView->setZoomFactor(0.9);
    m_mapView->hide();

    layout->addWidget(back);
    layout->addWidget(m_title);
    layout->addWidget(m_summary);
    layout->addLayout(actions);
    layout->addWidget(m_pileSection);
    layout->addWidget(m_status);
    layout->addWidget(m_piles, 1);
    layout->addWidget(m_navigationBar);
    layout->addWidget(m_mapView, 1);
    layout->addWidget(m_reserve);

    connect(back, &QPushButton::clicked, this, &StationDetailPage::backRequested);
    connect(m_favorite, &QPushButton::clicked, this, [this] {
        const bool next = !m_station.value(QStringLiteral("favorited")).toBool();
        setFavoriteState(next);
        emit favoriteRequested(m_station.value(QStringLiteral("stationId")).toInteger(), next);
    });
    connect(drive, &QPushButton::clicked, this,
            [this] { showNavigation(QStringLiteral("driving")); });
    connect(walk, &QPushButton::clicked, this,
            [this] { showNavigation(QStringLiteral("walking")); });
    connect(closeNavigation, &QPushButton::clicked, this,
            [this] { setNavigationMode(false); });
    connect(m_piles, &QListWidget::currentItemChanged, this,
            [this] { updateSelection(); });
    connect(m_reserve, &QPushButton::clicked, this, [this] {
        if (!m_piles->currentItem()) return;
        const QJsonObject pile = m_piles->currentItem()->data(Qt::UserRole).toJsonObject();
        if (pile.value(QStringLiteral("status")).toString() == QStringLiteral("available"))
            emit reservationRequested(m_station, pile);
    });
}

void StationDetailPage::setStation(const QJsonObject &station)
{
    setNavigationMode(false);
    if (!m_keepDirectPile) m_directPile = {};
    const qint64 stationId = station.value(QStringLiteral("stationId")).toInteger();
    if (stationId <= 0) {
        m_station = {};
        m_title->setText(QStringLiteral("未选择充电站"));
        m_summary->setText(QStringLiteral("请返回附近站点，选择一个有效充电站后再查看电桩和导航。"));
        m_status->setText(QStringLiteral("请先选择有效的充电站"));
        m_piles->clear();
        m_reserve->setEnabled(false);
        return;
    }
    m_station = station;
    m_title->setText(station.value(QStringLiteral("name")).toString());
    m_summary->setText(
        QStringLiteral("%1\n电价 ¥%2/度 · 空闲 %3/%4 · 距离 %5 km")
            .arg(station.value(QStringLiteral("address")).toString())
            .arg(station.value(QStringLiteral("priceCentsPerKwh")).toInt() / 100.0, 0, 'f', 2)
            .arg(station.value(QStringLiteral("availablePiles")).toInt())
            .arg(station.value(QStringLiteral("totalPiles")).toInt())
            .arg(station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 1));
    setFavoriteState(station.value(QStringLiteral("favorited")).toBool());
    m_status->setText(QStringLiteral("正在加载电桩实时状态…"));
    m_piles->clear();
    m_mapView->hide();
    m_reserve->setEnabled(false);
    emit pilesRequested(stationId);
}

void StationDetailPage::showNavigation(const QString &mode)
{
    if (m_station.value(QStringLiteral("stationId")).toInteger() <= 0) return;
    const bool walking = mode == QStringLiteral("walking");
    const double originLat = m_station.value(QStringLiteral("originLatitude")).toDouble(22.5431);
    const double originLon = m_station.value(QStringLiteral("originLongitude")).toDouble(114.0579);
    const QString originName = m_station.value(QStringLiteral("originName"))
                                   .toString(QStringLiteral("深圳市中心"));
    const QString stationName = m_station.value(QStringLiteral("name")).toString();
    m_navigationTitle->setText(QStringLiteral("%1路线：%2 → %3")
                                   .arg(walking ? QStringLiteral("步行") : QStringLiteral("驾车"),
                                        originName, stationName));
    const QUrl url = MapNavigator::navigationUrl(
        originLat, originLon,
        m_station.value(QStringLiteral("latitude")).toDouble(),
        m_station.value(QStringLiteral("longitude")).toDouble(), mode);
    if (url.isValid() && !url.isEmpty()) {
        m_mapView->setUrl(url);
    } else {
        const double distance = m_station.value(QStringLiteral("distanceKm")).toDouble();
        const int minutes = walking ? qMax(8, qRound(distance * 13))
                                    : qMax(4, qRound(distance * 4));
        m_mapView->setHtml(QStringLiteral(
            "<html><body style='margin:0;background:#eff6ff;font-family:sans-serif;color:#1e3a8a'>"
            "<div style='padding:28px'><h2>%1路线预览</h2><p>%2</p>"
            "<div style='border-left:5px solid #2563eb;min-height:160px;margin:20px 8px;padding-left:24px'>"
            "预计 %3 公里 · 约 %4 分钟<br><br>%5</div><p><b>%6</b></p>"
            "<small>配置腾讯地图 Key 后将加载真实路线页面</small></div></body></html>")
            .arg(walking ? QStringLiteral("步行") : QStringLiteral("驾车"), originName)
            .arg(distance, 0, 'f', 1).arg(minutes)
            .arg(walking ? QStringLiteral("优先选择人行道路") : QStringLiteral("已避开拥堵路段"),
                 stationName));
    }
    setNavigationMode(true);
    emit navigationRequested(m_station, mode);
}

void StationDetailPage::setNavigationMode(bool enabled)
{
    m_pileSection->setVisible(!enabled);
    m_status->setVisible(!enabled);
    m_piles->setVisible(!enabled);
    m_reserve->setVisible(!enabled);
    m_navigationBar->setVisible(enabled);
    m_mapView->setVisible(enabled);
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

void StationDetailPage::setPiles(const QJsonArray &piles)
{
    m_piles->clear();
    if (piles.isEmpty() && m_directPile.isEmpty()) {
        m_status->setText(QStringLiteral("该站点暂无电桩数据"));
        return;
    }
    int directRow = -1;
    const qint64 directId = m_directPile.value(QStringLiteral("pileId")).toInteger();
    for (const auto &value : piles) {
        const QJsonObject pile = value.toObject();
        addPileItem(m_piles, pile);
        if (pile.value(QStringLiteral("pileId")).toInteger() == directId)
            directRow = m_piles->count() - 1;
    }
    if (directId > 0 && directRow < 0) {
        addPileItem(m_piles, m_directPile);
        directRow = m_piles->count() - 1;
    }
    if (directRow >= 0) {
        m_piles->setCurrentRow(directRow);
        const QString code = m_directPile.value(QStringLiteral("pileCode")).toString();
        const bool available = m_directPile.value(QStringLiteral("status")).toString()
                               == QStringLiteral("available");
        m_status->setText(available
            ? QStringLiteral("已通过编号连接到 %1 · 当前空闲，可预约").arg(code)
            : QStringLiteral("已通过编号连接到 %1 · 当前状态：%2")
                  .arg(code, pileStatusText(
                      m_directPile.value(QStringLiteral("status")).toString())));
        return;
    }
    m_status->setText(QStringLiteral("状态更新时间：刚刚 · 共 %1 个电桩").arg(piles.size()));
}

void StationDetailPage::showError(const QString &message)
{
    m_status->setText(message);
    m_piles->clear();
    m_reserve->setEnabled(false);
}

void StationDetailPage::setFavoriteState(bool favorited)
{
    m_station.insert(QStringLiteral("favorited"), favorited);
    m_favorite->setText(favorited ? QStringLiteral("★ 已收藏")
                                  : QStringLiteral("☆ 收藏站点"));
}

void StationDetailPage::updateSelection()
{
    const auto *item = m_piles->currentItem();
    const bool available = item && item->data(Qt::UserRole).toJsonObject()
                                      .value(QStringLiteral("status")).toString()
                                  == QStringLiteral("available");
    m_reserve->setEnabled(available);
    m_reserve->setText(available ? QStringLiteral("预约此充电桩")
                                 : QStringLiteral("请选择空闲电桩"));
}
