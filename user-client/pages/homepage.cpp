#include "pages/homepage.h"
#include "pages/stationutils.h"
#include <QJsonObject>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVariant>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtMath>

namespace {
QString nonEmptyText(const QJsonObject &object, const QString &key, const QString &fallback)
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

QString stationPrice(const QJsonObject &station)
{
    if (station.value(QStringLiteral("priceCentsPerKwh")).isDouble()) {
        const double cents = station.value(QStringLiteral("priceCentsPerKwh")).toDouble();
        if (qIsFinite(cents) && cents >= 0.0)
            return QStringLiteral("¥%1/度").arg(cents / 100.0, 0, 'f', 2);
    }
    if (station.value(QStringLiteral("price")).isDouble()) {
        const double price = station.value(QStringLiteral("price")).toDouble();
        if (qIsFinite(price) && price >= 0.0)
            return QStringLiteral("¥%1/度").arg(price, 0, 'f', 2);
    }
    return QStringLiteral("电价未知");
}

QString pileAvailability(const QJsonObject &station)
{
    if (!station.value(QStringLiteral("availablePiles")).isDouble()
        || !station.value(QStringLiteral("totalPiles")).isDouble())
        return QStringLiteral("空闲桩数未知");
    const int available = station.value(QStringLiteral("availablePiles")).toInt(-1);
    const int total = station.value(QStringLiteral("totalPiles")).toInt(-1);
    if (available < 0 || total < 0 || available > total)
        return QStringLiteral("空闲桩数未知");
    return QStringLiteral("空闲 %1/%2").arg(available).arg(total);
}
}

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("homePage"));
    m_rootLayout = new QVBoxLayout(this); m_rootLayout->setContentsMargins(12,12,12,12); m_rootLayout->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("附近充电站"), this); QFont f=title->font(); f.setPointSize(19); f.setBold(true); title->setFont(f); m_rootLayout->addWidget(title);
    m_filterLayout = new QHBoxLayout; m_filterLayout->setObjectName(QStringLiteral("stationFilterLayout")); m_region=new QComboBox(this); m_region->setObjectName(QStringLiteral("stationRegionComboBox")); m_region->addItems({QStringLiteral("全部区域"),QStringLiteral("深圳市"),QStringLiteral("福田区"),QStringLiteral("罗湖区"),QStringLiteral("南山区"),QStringLiteral("宝安区"),QStringLiteral("龙岗区"),QStringLiteral("龙华区"),QStringLiteral("盐田区"),QStringLiteral("坪山区"),QStringLiteral("光明区"),QStringLiteral("大鹏新区")}); m_address=new QLineEdit(this); m_address->setObjectName(QStringLiteral("stationAddressInput")); m_address->setPlaceholderText(QStringLiteral("输入地址，如：深圳北站")); m_address->setMaxLength(200); m_filterLayout->addWidget(m_region); m_filterLayout->addWidget(m_address,1); m_rootLayout->addLayout(m_filterLayout);
    auto *geocodingRow = new QHBoxLayout; m_geocodingStatus=new QLabel(this); m_geocodingStatus->setObjectName(QStringLiteral("stationGeocodingStatus")); m_geocodingStatus->setWordWrap(true); m_geocodingRetryButton=new QPushButton(QStringLiteral("重新解析"),this); m_geocodingRetryButton->setObjectName(QStringLiteral("stationGeocodingRetryButton")); m_geocodingRetryButton->hide(); geocodingRow->addWidget(m_geocodingStatus,1); geocodingRow->addWidget(m_geocodingRetryButton); m_rootLayout->addLayout(geocodingRow);
    auto *gps=new QPushButton(QStringLiteral("使用模拟定位（深圳）"),this); m_searchButton=new QPushButton(QStringLiteral("搜索附近充电站"),this); m_searchButton->setObjectName(QStringLiteral("stationSearchButton")); m_rootLayout->addWidget(gps); m_rootLayout->addWidget(m_searchButton); m_status=new QLabel(this); m_status->setObjectName(QStringLiteral("stationSearchStatus")); m_status->setWordWrap(true); m_rootLayout->addWidget(m_status); m_list=new QListWidget(this); m_list->setObjectName(QStringLiteral("stationList")); m_list->setWordWrap(true); m_list->setSpacing(5); m_rootLayout->addWidget(m_list,1); renderEmpty(QStringLiteral("暂无站点数据，请搜索附近充电站"));
    connect(gps,&QPushButton::clicked,this,[this]{m_latitude=22.5431;m_longitude=114.0579;m_hasSearchLocation=true;m_simulatedLocationActive=true;m_address->setText(QStringLiteral("深圳市中心（模拟定位）"));clearGeocodingStatus();m_status->setText(QStringLiteral("已定位：22.5431, 114.0579"));}); connect(m_address,&QLineEdit::textEdited,this,[this]{m_simulatedLocationActive=false;}); connect(m_searchButton,&QPushButton::clicked,this,[this]{showLoading();emit stationsRequested(m_region->currentText(),m_simulatedLocationActive?QString():m_address->text().trimmed(),m_latitude,m_longitude);}); connect(m_address,&QLineEdit::returnPressed,m_searchButton,&QPushButton::click); connect(m_geocodingRetryButton,&QPushButton::clicked,this,[this]{showGeocodingLoading(m_address->text().trimmed());emit geocodingRetryRequested();}); connect(m_list,&QListWidget::itemClicked,this,[this](QListWidgetItem*i){const QJsonObject station=i->data(Qt::UserRole).toJsonObject();if(station.value(QStringLiteral("stationId")).toInteger()>0)emit stationSelected(station);}); }

void HomePage::renderEmpty(const QString &text){m_list->clear();auto*i=new QListWidgetItem(text,m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::clearStations(){m_hasSearchLocation=true;m_searchButton->setEnabled(true);m_searchButton->setText(QStringLiteral("搜索附近充电站"));m_list->clear();renderEmpty(QStringLiteral("暂无站点数据，请搜索附近充电站"));m_status->clear();}
void HomePage::showLoading(){m_searchButton->setEnabled(false);m_searchButton->setText(QStringLiteral("正在搜索…"));m_status->setText(QStringLiteral("正在查询附近充电站…"));m_list->clear();auto*i=new QListWidgetItem(QStringLiteral("加载中…"),m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showError(const QString &message){m_searchButton->setEnabled(true);m_searchButton->setText(QStringLiteral("重新搜索"));m_status->setText(message.isEmpty()?QStringLiteral("站点查询失败"):message);renderEmpty(QStringLiteral("暂时无法获取站点，请稍后重试"));}
void HomePage::setSearchLocation(double latitude, double longitude){m_latitude=latitude;m_longitude=longitude;m_hasSearchLocation=true;}
void HomePage::showTextSearchFallback(const QString &message){m_hasSearchLocation=false;m_status->setText(message);}
void HomePage::showGeocodingLoading(const QString &address){m_geocodingStatus->setStyleSheet(QStringLiteral("color:#1d4ed8;"));m_geocodingStatus->setText(QStringLiteral("正在解析地址：%1…").arg(address));m_geocodingRetryButton->hide();}
void HomePage::showGeocodingSucceeded(){m_geocodingStatus->setStyleSheet(QStringLiteral("color:#166534;"));m_geocodingStatus->setText(QStringLiteral("地址解析成功，正在按距离查询附近站点。"));m_geocodingRetryButton->hide();}
void HomePage::showGeocodingFailed(const QString &message){m_geocodingStatus->setStyleSheet(QStringLiteral("color:#b45309;"));m_geocodingStatus->setText(QStringLiteral("地址解析未完成：%1").arg(message));m_geocodingRetryButton->show();}
void HomePage::clearGeocodingStatus(){m_geocodingStatus->clear();m_geocodingRetryButton->hide();}
bool HomePage::searchLocation(double *latitude, double *longitude) const
{
    if (!m_hasSearchLocation) return false;
    if (latitude) *latitude = m_latitude;
    if (longitude) *longitude = m_longitude;
    return true;
}

void HomePage::setCompactLayout(bool compact)
{
    m_compactLayout = compact;
    m_filterLayout->setDirection(compact ? QBoxLayout::TopToBottom
                                         : QBoxLayout::LeftToRight);
    m_filterLayout->setSpacing(compact ? 8 : 12);
    m_rootLayout->setContentsMargins(compact ? 6 : 18, compact ? 6 : 14,
                                     compact ? 6 : 18, compact ? 6 : 14);
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->flags() != Qt::NoItemFlags)
            m_list->item(i)->setSizeHint(QSize(0, compact ? 110 : 92));
}
void HomePage::setStations(const QJsonArray &stations)
{
    m_searchButton->setEnabled(true);
    m_searchButton->setText(QStringLiteral("刷新站点"));
    m_list->clear();
    if (stations.isEmpty()) {
        renderEmpty(QStringLiteral("没有找到匹配的充电站"));
        m_status->setText(QStringLiteral("未找到匹配站点 · 更新于刚刚"));
        return;
    }
    const auto sorted = m_hasSearchLocation
        ? StationUtils::sortByDistance(stations, m_latitude, m_longitude) : stations;
    int rendered = 0;
    for (const auto &value : sorted) {
        if (!value.isObject()) continue;
        const auto station = value.toObject();
        const QString distance = m_hasSearchLocation
            && station.value(QStringLiteral("distanceKm")).isDouble()
            && qIsFinite(station.value(QStringLiteral("distanceKm")).toDouble())
            ? QStringLiteral("%1 km").arg(station.value(QStringLiteral("distanceKm")).toDouble(),0,'f',1)
            : QStringLiteral("距离未知");
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  %2\n%3\n%4    %5    %6")
                .arg(station.value(QStringLiteral("favorited")).toBool() ? QStringLiteral("★") : QStringLiteral("⚡"),
                     nonEmptyText(station, QStringLiteral("name"), QStringLiteral("未命名充电站")),
                     nonEmptyText(station, QStringLiteral("address"), QStringLiteral("地址暂未提供")),
                     stationPrice(station), pileAvailability(station), distance), m_list);
        item->setSizeHint(QSize(0, m_compactLayout ? 110 : 92));
        item->setData(Qt::UserRole,QVariant::fromValue(station));
        if (station.value(QStringLiteral("stationId")).toInteger() <= 0)
            item->setFlags(Qt::NoItemFlags);
        ++rendered;
    }
    if (rendered == 0) {
        renderEmpty(QStringLiteral("站点数据格式异常，请刷新重试"));
        m_status->setText(QStringLiteral("未能读取有效站点"));
        return;
    }
    m_status->setText(m_hasSearchLocation
        ? QStringLiteral("已找到 %1 个充电站，有效坐标按距离排序 · 更新于刚刚").arg(rendered)
        : QStringLiteral("已找到 %1 个文本匹配的充电站 · 更新于刚刚").arg(rendered));
}
void HomePage::updateFavoriteState(qint64 stationId, bool favorited)
{
    for (int i = 0; i < m_list->count(); ++i) {
        auto *item = m_list->item(i);
        QJsonObject station = item->data(Qt::UserRole).toJsonObject();
        if (station.value(QStringLiteral("stationId")).toInteger() != stationId) continue;
        station.insert(QStringLiteral("favorited"), favorited);
        item->setData(Qt::UserRole, station);
        const QString text = item->text();
        const qsizetype separator = text.indexOf(QStringLiteral("  "));
        if (separator >= 0)
            item->setText((favorited ? QStringLiteral("★") : QStringLiteral("⚡"))
                          + text.mid(separator));
        break;
    }
}
