#include "pages/homepage.h"

#include "pages/stationutils.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("附近充电站"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *locationRow = new QHBoxLayout;
    m_region = new QComboBox(this);
    m_region->setObjectName(QStringLiteral("regionCombo"));
    m_region->addItems({QStringLiteral("全部区域"),
                        QStringLiteral("浦东新区"),
                        QStringLiteral("徐汇区"),
                        QStringLiteral("静安区")});
    m_address = new QLineEdit(this);
    m_address->setObjectName(QStringLiteral("addressEdit"));
    m_address->setPlaceholderText(QStringLiteral("输入详细地址（1—200字）"));
    m_address->setMaxLength(200);
    m_address->setInputMethodHints(Qt::ImhNone);
    m_address->setAttribute(Qt::WA_InputMethodEnabled, true);
    locationRow->addWidget(m_region);
    locationRow->addWidget(m_address, 1);
    layout->addLayout(locationRow);

    m_gpsButton = new QPushButton(
        QStringLiteral("使用模拟定位（上海人民广场）"), this);
    m_gpsButton->setObjectName(QStringLiteral("gpsButton"));
    m_searchButton = new QPushButton(QStringLiteral("解析地址并搜索"), this);
    m_searchButton->setObjectName(QStringLiteral("searchButton"));
    layout->addWidget(m_gpsButton);
    layout->addWidget(m_searchButton);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("homeStatusLabel"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);
    renderEmpty(QStringLiteral("暂无站点数据，请先输入地址或使用模拟定位"));

    connect(m_gpsButton, &QPushButton::clicked, this, [this] {
        m_latitude = 31.2304;
        m_longitude = 121.4737;
        m_address->setText(QStringLiteral("上海市人民广场（模拟定位）"));
        m_hasResolvedLocation = true;
        m_resolvedQuery = currentLocationQuery();
        m_status->setText(
            QStringLiteral("已使用模拟定位：31.230400, 121.473700（GCJ-02）"));
    });
    connect(m_address, &QLineEdit::textEdited, this, [this] {
        if (currentLocationQuery() != m_resolvedQuery)
            m_hasResolvedLocation = false;
    });
    connect(m_region, &QComboBox::currentTextChanged, this, [this] {
        if (currentLocationQuery() != m_resolvedQuery)
            m_hasResolvedLocation = false;
    });
    connect(m_searchButton, &QPushButton::clicked, this, [this] {
        const QString address = m_address->text().trimmed();
        if (address.isEmpty()) {
            m_status->setText(QStringLiteral("请先输入地址或使用模拟定位后再搜索"));
            renderEmpty(QStringLiteral("搜索位置不能为空"));
            return;
        }
        if (m_hasResolvedLocation
            && currentLocationQuery() == m_resolvedQuery) {
            requestStationsFromResolvedLocation();
            return;
        }

        setLocationControlsBusy(true);
        m_status->setText(QStringLiteral("正在通过腾讯地图解析地址…"));
        emit geocodeRequested(address, selectedRegion());
    });
    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) {
        emit stationSelected(item->data(Qt::UserRole).toJsonObject());
    });
}

void HomePage::renderEmpty(const QString &text)
{
    m_list->clear();
    auto *item = new QListWidgetItem(text, m_list);
    item->setFlags(Qt::NoItemFlags);
}

void HomePage::setLocationControlsBusy(bool busy)
{
    m_region->setDisabled(busy);
    m_address->setDisabled(busy);
    m_gpsButton->setDisabled(busy);
    m_searchButton->setDisabled(busy);
}

QString HomePage::selectedRegion() const
{
    return m_region->currentIndex() == 0
        ? QString()
        : m_region->currentText().trimmed();
}

QString HomePage::currentLocationQuery() const
{
    return selectedRegion() + QLatin1Char('\n') + m_address->text().trimmed();
}

void HomePage::requestStationsFromResolvedLocation()
{
    if (!m_hasResolvedLocation) return;
    showLoading();
    emit stationsRequested(selectedRegion(), m_address->text().trimmed(),
                           m_latitude, m_longitude);
}

void HomePage::setResolvedLocation(double latitude, double longitude,
                                   const QString &formattedAddress)
{
    if (!std::isfinite(latitude) || !std::isfinite(longitude)
        || latitude < -90.0 || latitude > 90.0
        || longitude < -180.0 || longitude > 180.0) {
        showAddressResolutionError(QStringLiteral("腾讯地图返回的坐标无效"));
        return;
    }

    m_latitude = latitude;
    m_longitude = longitude;
    if (!formattedAddress.trimmed().isEmpty())
        m_address->setText(formattedAddress.trimmed());
    m_hasResolvedLocation = true;
    m_resolvedQuery = currentLocationQuery();
    m_status->setText(
        QStringLiteral("地址解析成功：%1, %2（GCJ-02）")
            .arg(latitude, 0, 'f', 6)
            .arg(longitude, 0, 'f', 6));
    requestStationsFromResolvedLocation();
}

void HomePage::showAddressResolutionError(const QString &message)
{
    setLocationControlsBusy(false);
    m_hasResolvedLocation = false;
    m_status->setText(
        QStringLiteral("%1\n可修改地址后重试，或使用模拟定位继续查找站点。")
            .arg(message.isEmpty()
                     ? QStringLiteral("地址解析失败")
                     : message));
    if (m_stations.isEmpty())
        renderEmpty(QStringLiteral("地址解析失败，尚未发起站点查询"));
}

void HomePage::useTextSearchFallback()
{
    m_hasResolvedLocation = false;
    m_resolvedQuery = currentLocationQuery();
    setLocationControlsBusy(true);
    m_status->setText(QStringLiteral(
        "服务端未提供地址解析，正在按站点名称或地址直接搜索…"));
    renderEmpty(QStringLiteral("加载中…"));
    emit stationsRequested(selectedRegion(), m_address->text().trimmed(),
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN());
}

void HomePage::showLoading()
{
    setLocationControlsBusy(true);
    m_status->setText(QStringLiteral("正在查询附近充电站…"));
    renderEmpty(QStringLiteral("加载中…"));
}

void HomePage::showError(const QString &message)
{
    setLocationControlsBusy(false);
    m_status->setText(message);
    renderEmpty(QStringLiteral("暂时无法获取站点，请稍后重试"));
}

void HomePage::setStations(const QJsonArray &stations)
{
    setLocationControlsBusy(false);
    m_stations = m_hasResolvedLocation
        ? StationUtils::sortByDistance(stations, m_latitude, m_longitude)
        : stations;
    renderStations();
}

void HomePage::applyFavorite(qint64 stationId, bool favorited)
{
    for (qsizetype index = 0; index < m_stations.size(); ++index) {
        QJsonObject station = m_stations.at(index).toObject();
        if (station.value(QStringLiteral("stationId")).toVariant().toLongLong()
            != stationId) continue;
        station.insert(QStringLiteral("favorited"), favorited);
        m_stations.replace(index, station);
        renderStations();
        return;
    }
}

void HomePage::renderStations()
{
    m_list->clear();
    if (m_stations.isEmpty()) {
        renderEmpty(QStringLiteral("附近暂无可用充电站"));
        m_status->setText(QStringLiteral("当前范围内没有可用充电站"));
        return;
    }
    for (const QJsonValue &value : m_stations) {
        const QJsonObject station = value.toObject();
        const double price = station.contains(QStringLiteral("priceCentsPerKwh"))
            ? station.value(QStringLiteral("priceCentsPerKwh")).toDouble() / 100.0
            : station.value(QStringLiteral("price")).toDouble();
        const QString favoriteMark =
            station.value(QStringLiteral("favorited")).toBool()
            ? QStringLiteral("★ ") : QString();
        const QJsonValue distanceValue =
            station.value(QStringLiteral("distanceKm"));
        const QString distanceText = distanceValue.isDouble()
            ? QStringLiteral(" · %1 km").arg(distanceValue.toDouble(), 0, 'f', 1)
            : QString();
        auto *item = new QListWidgetItem(
            QStringLiteral("%1%2\n%3\n电价 ¥%4/度 · 空闲 %5/%6%7%8")
                .arg(favoriteMark,
                     station.value(QStringLiteral("name")).toString(),
                     station.value(QStringLiteral("address")).toString())
                .arg(price, 0, 'f', 2)
                .arg(station.value(QStringLiteral("availablePiles")).toInt())
                .arg(station.value(QStringLiteral("totalPiles")).toInt())
                .arg(distanceText)
                .arg(station.value(QStringLiteral("favorited")).toBool()
                         ? QStringLiteral(" · 已收藏") : QString()),
            m_list);
        item->setData(Qt::UserRole, QVariant::fromValue(station));
    }
    m_status->setText(m_hasResolvedLocation
        ? QStringLiteral("已找到 %1 个充电站，按距离排序")
              .arg(m_stations.size())
        : QStringLiteral("按名称或地址找到 %1 个充电站")
              .arg(m_stations.size()));
}
