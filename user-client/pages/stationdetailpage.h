#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
class QLabel; class QListWidget; class QPushButton;
class StationDetailPage final : public QWidget { Q_OBJECT
public: explicit StationDetailPage(QWidget *parent=nullptr); void setStation(const QJsonObject&); void setPiles(const QJsonArray&); void showError(const QString&);
signals: void backRequested(); void pilesRequested(qint64); void favoriteRequested(qint64,bool);
private: QJsonObject m_station; QLabel *m_title=nullptr,*m_summary=nullptr,*m_status=nullptr; QListWidget *m_piles=nullptr; QPushButton *m_favorite=nullptr;
};
