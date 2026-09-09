#include "pages/homepage.h"
#include "pages/stationutils.h"
#include "map/mapnavigator.h"
#include <QJsonObject>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QVariant>
#include <QHBoxLayout>
#include <QVBoxLayout>

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("homePage"));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(12,12,12,12); layout->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("附近充电站"), this); QFont f=title->font(); f.setPointSize(19); f.setBold(true); title->setFont(f); layout->addWidget(title);
    auto *row = new QHBoxLayout;
    m_region = new QComboBox(this);
    m_region->addItems({QStringLiteral("全部区域"), QStringLiteral("深圳市")});
    m_address = new QLineEdit(this);
    m_address->setPlaceholderText(QStringLiteral("输入地址"));
    row->addWidget(m_region);
    row->addWidget(m_address, 1);
    layout->addLayout(row);

    auto *gps = new QPushButton(QStringLiteral("使用模拟定位（深圳）"), this);
    auto *search = new QPushButton(QStringLiteral("搜索附近充电站"), this);
    search->setObjectName(QStringLiteral("stationSearchButton"));
    layout->addWidget(gps);
    layout->addWidget(search);

    auto *codeRow = new QHBoxLayout;
    m_pileCode = new QLineEdit(this);
    m_pileCode->setObjectName(QStringLiteral("pileCodeInput"));
    m_pileCode->setPlaceholderText(QStringLiteral("已到桩旁？输入电桩编号"));
    m_pileCode->setMaxLength(64);
    auto *connectPile = new QPushButton(QStringLiteral("编号直达"), this);
    connectPile->setObjectName(QStringLiteral("pileCodeConnectButton"));
    codeRow->addWidget(m_pileCode, 1);
    codeRow->addWidget(connectPile);
    layout->addLayout(codeRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("stationList"));
    m_list->setSpacing(5);
    layout->addWidget(m_list, 1);
    renderEmpty(QStringLiteral("暂无站点数据，请搜索附近充电站"));
    m_network = new QNetworkAccessManager(this);

    connect(gps, &QPushButton::clicked, this, [this] {
        cancelGeocoding();
        m_latitude = 22.5431;
        m_longitude = 114.0579;
        m_simulatedLocationActive = true;
        m_address->setText(QStringLiteral("深圳市中心（模拟定位）"));
        m_status->setText(QStringLiteral("已定位：22.5431, 114.0579"));
    });
    connect(m_address, &QLineEdit::textEdited, this,
            [this] {
                cancelGeocoding();
                m_simulatedLocationActive = false;
            });
    connect(search, &QPushButton::clicked, this, &HomePage::requestStations);
    const auto submitPileCode = [this] {
        const QString pileCode = m_pileCode->text().trimmed();
        if (pileCode.isEmpty()) {
            showPileLookupError(QStringLiteral("请输入电桩编号"));
            return;
        }
        m_status->setText(QStringLiteral("正在连接电桩 %1…").arg(pileCode));
        emit pileCodeRequested(pileCode);
    };
    connect(connectPile, &QPushButton::clicked, this, submitPileCode);
    connect(m_pileCode, &QLineEdit::returnPressed, this, submitPileCode);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QJsonObject station = item->data(Qt::UserRole).toJsonObject();
        station.insert(QStringLiteral("originLatitude"), m_latitude);
        station.insert(QStringLiteral("originLongitude"), m_longitude);
        station.insert(QStringLiteral("originName"), m_address->text().trimmed().isEmpty()
                           ? QStringLiteral("当前位置") : m_address->text().trimmed());
        if (station.value(QStringLiteral("stationId")).toInteger() > 0)
            emit stationSelected(station);
    });
}

void HomePage::requestStations()
{
    const QString address = m_address->text().trimmed();
    showLoading();
    if (address.isEmpty() || m_simulatedLocationActive) {
        emit stationsRequested(m_region->currentText(), QString(), m_latitude, m_longitude);
        return;
    }
    if (!MapNavigator::isConfigured()) {
        showError(QStringLiteral("未配置腾讯地图 Key，无法将地址转换为坐标"));
        return;
    }
    cancelGeocoding();
    const QUrl url = MapNavigator::geocodingUrl(address, m_region->currentText());
    m_status->setText(QStringLiteral("正在通过腾讯地图解析地址…"));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    m_geocodingReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, address, reply] {
        if (m_geocodingReply != reply) {
            reply->deleteLater();
            return;
        }
        m_geocodingReply = nullptr;
        const auto networkError = reply->error();
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            showError(QStringLiteral("地址解析失败：网络连接异常，请重试"));
            return;
        }
        QString error;
        double latitude = 0.0;
        double longitude = 0.0;
        if (!MapNavigator::parseGeocodingResponse(body, &latitude, &longitude, &error)) {
            showError(QStringLiteral("地址解析失败：%1").arg(error));
            return;
        }
        m_latitude = latitude;
        m_longitude = longitude;
        m_simulatedLocationActive = false;
        m_status->setText(QStringLiteral("已定位 %1：%2, %3；正在查询附近站点…")
                              .arg(address)
                              .arg(latitude, 0, 'f', 6)
                              .arg(longitude, 0, 'f', 6));
        // 地址用于确定查询起点；转换成功后按坐标找附近站点，不再将地址当作站名过滤条件。
        emit stationsRequested(m_region->currentText(), QString(), latitude, longitude);
    });
}

void HomePage::cancelGeocoding()
{
    if (!m_geocodingReply) return;
    QNetworkReply *reply = m_geocodingReply;
    m_geocodingReply = nullptr;
    reply->abort();
    reply->deleteLater();
}

void HomePage::renderEmpty(const QString &text){m_list->clear();auto*i=new QListWidgetItem(text,m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showLoading(){m_status->setText(QStringLiteral("正在查询附近充电站…"));m_list->clear();auto*i=new QListWidgetItem(QStringLiteral("加载中…"),m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showError(const QString &message){m_status->setText(message);renderEmpty(QStringLiteral("暂时无法获取站点，请稍后重试"));}
void HomePage::showPileLookupError(const QString &message)
{
    m_status->setText(QStringLiteral("编号连接失败：%1").arg(message));
}
void HomePage::setStations(const QJsonArray &stations){m_list->clear();if(stations.isEmpty()){renderEmpty(QStringLiteral("附近暂无可用充电站"));return;}const auto sorted=StationUtils::sortByDistance(stations,m_latitude,m_longitude);for(const auto&v:sorted){const auto o=v.toObject();const double price=o.contains(QStringLiteral("priceCentsPerKwh"))?o.value(QStringLiteral("priceCentsPerKwh")).toDouble()/100.0:o.value(QStringLiteral("price")).toDouble();auto*i=new QListWidgetItem(QStringLiteral("%1  %2\n%3\n¥%4/度    空闲 %5/%6    %7 km").arg(o.value("favorited").toBool()?QStringLiteral("★"):QStringLiteral("⚡"),o.value("name").toString(),o.value("address").toString()).arg(price,0,'f',2).arg(o.value("availablePiles").toInt()).arg(o.value("totalPiles").toInt()).arg(o.value("distanceKm").toDouble(),0,'f',1),m_list);i->setSizeHint(QSize(0,92));i->setData(Qt::UserRole,QVariant::fromValue(o));}m_status->setText(QStringLiteral("已找到 %1 个充电站，按距离为您排序").arg(sorted.size()));}
