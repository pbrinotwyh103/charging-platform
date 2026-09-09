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
    void setDirectPile(const QJsonObject &station, const QJsonObject &pile);
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
    void showNavigation(const QString &mode);
    void setNavigationMode(bool enabled);

    QJsonObject m_station;
    QJsonObject m_directPile;
    bool m_keepDirectPile = false;
    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_pileSection = nullptr;
    QLabel *m_navigationTitle = nullptr;
    QListWidget *m_piles = nullptr;
    QPushButton *m_favorite = nullptr;
    QPushButton *m_reserve = nullptr;
    QWidget *m_navigationBar = nullptr;
    QWebEngineView *m_mapView = nullptr;
};
