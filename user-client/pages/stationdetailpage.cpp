#include "pages/stationdetailpage.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>
StationDetailPage::StationDetailPage(QWidget*p):QWidget(p){auto*l=new QVBoxLayout(this);auto*b=new QPushButton(QStringLiteral("返回附近站点"),this);l->addWidget(b);m_title=new QLabel(this);l->addWidget(m_title);m_summary=new QLabel(this);m_summary->setWordWrap(true);l->addWidget(m_summary);m_favorite=new QPushButton(this);l->addWidget(m_favorite);m_status=new QLabel(this);l->addWidget(m_status);m_piles=new QListWidget(this);l->addWidget(m_piles,1);connect(b,&QPushButton::clicked,this,&StationDetailPage::backRequested);connect(m_favorite,&QPushButton::clicked,this,[this]{emit favoriteRequested(m_station.value("stationId").toInteger(),!m_station.value("favorited").toBool());});}
void StationDetailPage::setStation(const QJsonObject&s){m_station=s;m_title->setText(s.value("name").toString());m_summary->setText(QStringLiteral("%1\n电价 ¥%2/度 · 空闲 %3/%4").arg(s.value("address").toString()).arg(s.value("priceCentsPerKwh").toInt()/100.0,0,'f',2).arg(s.value("availablePiles").toInt()).arg(s.value("totalPiles").toInt()));m_favorite->setText(s.value("favorited").toBool()?QStringLiteral("取消收藏"):QStringLiteral("收藏站点"));m_status->setText(QStringLiteral("正在加载电桩信息…"));emit pilesRequested(s.value("stationId").toInteger());}
void StationDetailPage::setPiles(const QJsonArray&p){m_piles->clear();if(p.isEmpty()){m_status->setText(QStringLiteral("暂无电桩数据"));return;}for(const auto&v:p){auto o=v.toObject();new QListWidgetItem(QStringLiteral("%1 · %2 · %3 kW\n状态：%4").arg(o.value("pileCode").toString(),o.value("type").toString()).arg(o.value("powerKw").toDouble()).arg(o.value("status").toString()),m_piles);}m_status->setText(QStringLiteral("共 %1 个电桩").arg(p.size()));}
void StationDetailPage::showError(const QString&m){m_status->setText(m);m_piles->clear();}
