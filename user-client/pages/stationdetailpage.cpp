#include "pages/stationdetailpage.h"
#include "map/mapnavigator.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
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
    auto *walk = new QPushButton(QStringLiteral("步行导航"), this);
    actions->addWidget(m_favorite);
    actions->addWidget(drive);
    actions->addWidget(walk);

    auto *section = new QLabel(QStringLiteral("选择可用充电桩"), this);
    QFont sectionFont = section->font();
    sectionFont.setBold(true);
    section->setFont(sectionFont);
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
    m_mapView = new QWebEngineView(this);
    m_mapView->setObjectName(QStringLiteral("navigationWebView"));
    m_mapView->setMinimumHeight(210);
    m_mapView->hide();

    layout->addWidget(back);
    layout->addWidget(m_title);
    layout->addWidget(m_summary);
    layout->addLayout(actions);
    layout->addWidget(section);
    layout->addWidget(m_status);
    layout->addWidget(m_piles, 1);
    layout->addWidget(m_mapView);
    layout->addWidget(m_reserve);

    connect(back, &QPushButton::clicked, this, &StationDetailPage::backRequested);
    connect(m_favorite, &QPushButton::clicked, this, [this] {
        const bool next = !m_station.value(QStringLiteral("favorited")).toBool();
        setFavoriteState(next);
        emit favoriteRequested(m_station.value(QStringLiteral("stationId")).toInteger(), next);
    });
    connect(drive, &QPushButton::clicked, this, [this] {
        const QUrl url = MapNavigator::navigationUrl(
            22.5431, 114.0579,
            m_station.value(QStringLiteral("latitude")).toDouble(),
            m_station.value(QStringLiteral("longitude")).toDouble(),
            QStringLiteral("driving"));
        if (url.isValid() && !url.isEmpty()) m_mapView->setUrl(url);
        else m_mapView->setHtml(QStringLiteral(
            "<html><body style='margin:0;background:#eff6ff;font-family:sans-serif;color:#1e3a8a'>"
            "<div style='padding:18px'><b>驾车路线预览</b><p>当前位置（深圳市中心）</p>"
            "<div style='border-left:4px solid #2563eb;height:70px;margin-left:8px;padding-left:18px'>"
            "预计 %1 公里 · 约 %2 分钟<br>已避开拥堵路段</div><p><b>%3</b></p>"
            "<small>配置腾讯地图Key后将加载真实路线页面</small></div></body></html>")
            .arg(m_station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 1)
            .arg(qMax(4, qRound(m_station.value(QStringLiteral("distanceKm")).toDouble() * 4)))
            .arg(m_station.value(QStringLiteral("name")).toString()));
        m_mapView->show();
        emit navigationRequested(m_station, QStringLiteral("driving"));
    });
    connect(walk, &QPushButton::clicked, this, [this] {
        const QUrl url = MapNavigator::navigationUrl(
            22.5431, 114.0579,
            m_station.value(QStringLiteral("latitude")).toDouble(),
            m_station.value(QStringLiteral("longitude")).toDouble(),
            QStringLiteral("walking"));
        if (url.isValid() && !url.isEmpty()) m_mapView->setUrl(url);
        else m_mapView->setHtml(QStringLiteral(
            "<html><body style='margin:0;background:#f0fdf4;font-family:sans-serif;color:#166534'>"
            "<div style='padding:18px'><b>步行路线预览</b><p>当前位置（深圳市中心）</p>"
            "<div style='border-left:4px solid #16a34a;height:70px;margin-left:8px;padding-left:18px'>"
            "预计 %1 公里 · 约 %2 分钟<br>优先选择人行道路</div><p><b>%3</b></p>"
            "<small>配置腾讯地图Key后将加载真实路线页面</small></div></body></html>")
            .arg(m_station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 1)
            .arg(qMax(8, qRound(m_station.value(QStringLiteral("distanceKm")).toDouble() * 13)))
            .arg(m_station.value(QStringLiteral("name")).toString()));
        m_mapView->show();
        emit navigationRequested(m_station, QStringLiteral("walking"));
    });
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
    const qint64 stationId = station.value(QStringLiteral("stationId")).toInteger();
    if (stationId <= 0) {
        m_station = {};
        m_title->setText(QStringLiteral("未选择充电站"));
        m_summary->setText(QStringLiteral("请返回附近站点，选择一个有效充电站后再查看电桩和导航。"));
        m_status->setText(QStringLiteral("请先选择有效的充电站"));
        m_piles->clear();
        m_mapView->hide();
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
    m_status->setText(QStringLiteral("正在加载电桩运行状态…"));
    m_piles->clear();
    m_mapView->hide();
    m_reserve->setEnabled(false);
    emit pilesRequested(stationId);
}

void StationDetailPage::setPiles(const QJsonArray &piles)
{
    m_piles->clear();
    if (piles.isEmpty()) {
        m_status->setText(QStringLiteral("该站点暂无电桩数据"));
        return;
    }
    for (const auto &value : piles) {
        const QJsonObject pile = value.toObject();
        const bool available = pile.value(QStringLiteral("status")).toString()
                               == QStringLiteral("available");
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  ·  %2  ·  %3 kW\n%4%5")
                .arg(pile.value(QStringLiteral("pileCode")).toString(),
                     pileTypeText(pile.value(QStringLiteral("type")).toString()))
                .arg(pile.value(QStringLiteral("powerKw")).toDouble(), 0, 'f', 0)
                .arg(pileStatusText(pile.value(QStringLiteral("status")).toString()),
                     available ? QStringLiteral(" · 可立即预约") : QString()),
            m_piles);
        item->setData(Qt::UserRole, QVariant::fromValue(pile));
        if (!available)
            item->setForeground(QColor(QStringLiteral("#94a3b8")));
    }
    m_status->setText(
        QStringLiteral("共 %1 个电桩 · 运行状态为演示模拟数据，非设备实时状态")
            .arg(piles.size()));
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
