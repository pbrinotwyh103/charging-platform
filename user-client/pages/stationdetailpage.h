#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QBoxLayout;
class QVBoxLayout;

class StationDetailPage final : public QWidget
{
    Q_OBJECT

public:
    explicit StationDetailPage(QWidget *parent = nullptr);
    void setStation(const QJsonObject &station);
    void clearStation();
    qint64 stationId() const { return m_station.value(QStringLiteral("stationId")).toInteger(); }
    void setPiles(const QJsonArray &piles);
    void setDemoMode(bool demo) { m_demoMode = demo; }
    void showError(const QString &message);
    void setFavoriteState(bool favorited);
    void favoriteUpdateSucceeded(bool favorited);
    void favoriteUpdateFailed(const QString &message);
    void setCompactLayout(bool compact);

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
    QPushButton *m_refresh = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QBoxLayout *m_actionLayout = nullptr;
    bool m_favoriteBeforeRequest = false;
    bool m_demoMode = false;
};
