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
    auto *row = new QHBoxLayout; m_region=new QComboBox(this); m_region->addItems({QStringLiteral("全部区域"),QStringLiteral("浦东新区"),QStringLiteral("徐汇区"),QStringLiteral("静安区")}); m_address=new QLineEdit(this); m_address->setPlaceholderText(QStringLiteral("输入地址")); row->addWidget(m_region); row->addWidget(m_address,1); layout->addLayout(row);
    auto *gps=new QPushButton(QStringLiteral("使用模拟定位（上海）"),this); auto *search=new QPushButton(QStringLiteral("搜索附近充电站"),this); search->setObjectName(QStringLiteral("stationSearchButton")); layout->addWidget(gps); layout->addWidget(search); m_status=new QLabel(this); m_status->setWordWrap(true); layout->addWidget(m_status); m_list=new QListWidget(this); m_list->setObjectName(QStringLiteral("stationList")); m_list->setSpacing(5); layout->addWidget(m_list,1); renderEmpty(QStringLiteral("暂无站点数据，请搜索附近充电站"));
    connect(gps,&QPushButton::clicked,this,[this]{m_latitude=31.2304;m_longitude=121.4737;m_address->setText(QStringLiteral("上海市人民广场（模拟定位）"));m_status->setText(QStringLiteral("已定位：31.2304, 121.4737"));}); connect(search,&QPushButton::clicked,this,[this]{showLoading();emit stationsRequested(m_region->currentText(),m_address->text().trimmed(),m_latitude,m_longitude);}); connect(m_list,&QListWidget::itemClicked,this,[this](QListWidgetItem*i){emit stationSelected(i->data(Qt::UserRole).toJsonObject());}); }

void HomePage::renderEmpty(const QString &text){m_list->clear();auto*i=new QListWidgetItem(text,m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showLoading(){m_status->setText(QStringLiteral("正在查询附近充电站…"));m_list->clear();auto*i=new QListWidgetItem(QStringLiteral("加载中…"),m_list);i->setFlags(Qt::NoItemFlags);}
void HomePage::showError(const QString &message){m_status->setText(message);renderEmpty(QStringLiteral("暂时无法获取站点，请稍后重试"));}
void HomePage::setStations(const QJsonArray &stations){m_list->clear();if(stations.isEmpty()){renderEmpty(QStringLiteral("附近暂无可用充电站"));return;}const auto sorted=StationUtils::sortByDistance(stations,m_latitude,m_longitude);for(const auto&v:sorted){const auto o=v.toObject();const double price=o.contains(QStringLiteral("priceCentsPerKwh"))?o.value(QStringLiteral("priceCentsPerKwh")).toDouble()/100.0:o.value(QStringLiteral("price")).toDouble();auto*i=new QListWidgetItem(QStringLiteral("%1  %2\n%3\n¥%4/度    空闲 %5/%6    %7 km").arg(o.value("favorited").toBool()?QStringLiteral("★"):QStringLiteral("⚡"),o.value("name").toString(),o.value("address").toString()).arg(price,0,'f',2).arg(o.value("availablePiles").toInt()).arg(o.value("totalPiles").toInt()).arg(o.value("distanceKm").toDouble(),0,'f',1),m_list);i->setSizeHint(QSize(0,92));i->setData(Qt::UserRole,QVariant::fromValue(o));}m_status->setText(QStringLiteral("已找到 %1 个充电站，按距离为您排序").arg(sorted.size()));}
