#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QWebEngineView;

class StationDetailPage final : public QWidget
{
    Q_OBJECT

public:
    explicit StationDetailPage(QWidget *parent = nullptr);
    void setStation(const QJsonObject &station);
    void setPiles(const QJsonArray &piles);
    void showError(const QString &message);
    void setFavoriteState(bool favorited);

signals:
    void backRequested();
    void pilesRequested(qint64 stationId);
    void favoriteRequested(qint64 stationId, bool favorited);
    void navigationRequested(const QJsonObject &station, const QString &mode);
    void reservationRequested(const QJsonObject &station, const QJsonObject &pile);

private:
    void updateSelection();

    QJsonObject m_station;
    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_status = nullptr;
    QListWidget *m_piles = nullptr;
    QPushButton *m_favorite = nullptr;
    QPushButton *m_reserve = nullptr;
    QWebEngineView *m_mapView = nullptr;
};
