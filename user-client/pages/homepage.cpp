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

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("homePage"));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(12,12,12,12); layout->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("附近充电站"), this); QFont f=title->font(); f.setPointSize(19); f.setBold(true); title->setFont(f); layout->addWidget(title);
    auto *row = new QHBoxLayout; m_region=new QComboBox(this); m_region->setObjectName(QStringLiteral("stationRegionComboBox")); m_region->addItems({QStringLiteral("全部区域"),QStringLiteral("深圳市"),QStringLiteral("福田区"),QStringLiteral("罗湖区"),QStringLiteral("南山区"),QStringLiteral("宝安区"),QStringLiteral("龙岗区"),QStringLiteral("龙华区"),QStringLiteral("盐田区"),QStringLiteral("坪山区"),QStringLiteral("光明区"),QStringLiteral("大鹏新区")}); m_address=new QLineEdit(this); m_address->setObjectName(QStringLiteral("stationAddressInput")); m_address->setPlaceholderText(QStringLiteral("输入地址，如：深圳北站")); m_address->setInputMethodHints(Qt::ImhNone); m_address->setAttribute(Qt::WA_InputMethodEnabled, true); row->addWidget(m_region); row->addWidget(m_address,1); layout->addLayout(row);
    auto *gps=new QPushButton(QStringLiteral("使用模拟定位（深圳）"),this); auto *search=new QPushButton(QStringLiteral("搜索附近充电站"),this); search->setObjectName(QStringLiteral("stationSearchButton")); layout->addWidget(gps); layout->addWidget(search); m_status=new QLabel(this); m_status->setWordWrap(true); layout->addWidget(m_status); m_list=new QListWidget(this); m_list->setObjectName(QStringLiteral("stationList")); m_list->setSpacing(5); layout->addWidget(m_list,1); renderEmpty(QStringLiteral("暂无站点数据，请搜索附近充电站"));
    connect(gps,&QPushButton::clicked,this,[this]{m_latitude=22.5431;m_longitude=114.0579;m_simulatedLocationActive=true;m_address->setText(QStringLiteral("深圳市中心（模拟定位）"));m_status->setText(QStringLiteral("已定位：22.5431, 114.0579"));}); connect(m_address,&QLineEdit::textEdited,this,[this]{m_simulatedLocationActive=false;}); connect(search,&QPushButton::clicked,this,[this]{showLoading();emit stationsRequested(m_region->currentText(),m_simulatedLocationActive?QString():m_address->text().trimmed(),m_latitude,m_longitude);}); connect(m_list,&QListWidget::itemClicked,this,[this](QListWidgetItem*i){const QJsonObject station=i->data(Qt::UserRole).toJsonObject();if(station.value(QStringLiteral("stationId")).toInteger()>0)emit stationSelected(station);}); }

void HomePage::renderEmpty(const QString &text){m_list->clear();auto*i=new QListWidgetItem(text,m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showLoading(){m_status->setText(QStringLiteral("正在查询附近充电站…"));m_list->clear();auto*i=new QListWidgetItem(QStringLiteral("加载中…"),m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showError(const QString &message){m_status->setText(message);renderEmpty(QStringLiteral("暂时无法获取站点，请稍后重试"));}
void HomePage::setStations(const QJsonArray &stations){m_list->clear();if(stations.isEmpty()){renderEmpty(QStringLiteral("附近暂无可用充电站"));return;}const auto sorted=StationUtils::sortByDistance(stations,m_latitude,m_longitude);for(const auto&v:sorted){const auto o=v.toObject();const double price=o.contains(QStringLiteral("priceCentsPerKwh"))?o.value(QStringLiteral("priceCentsPerKwh")).toDouble()/100.0:o.value(QStringLiteral("price")).toDouble();auto*i=new QListWidgetItem(QStringLiteral("%1  %2\n%3\n¥%4/度    空闲 %5/%6    %7 km").arg(o.value("favorited").toBool()?QStringLiteral("★"):QStringLiteral("⚡"),o.value("name").toString(),o.value("address").toString()).arg(price,0,'f',2).arg(o.value("availablePiles").toInt()).arg(o.value("totalPiles").toInt()).arg(o.value("distanceKm").toDouble(),0,'f',1),m_list);i->setSizeHint(QSize(0,92));i->setData(Qt::UserRole,QVariant::fromValue(o));}m_status->setText(QStringLiteral("已找到 %1 个充电站，按距离为您排序").arg(sorted.size()));}
