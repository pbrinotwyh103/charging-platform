#pragma once

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

class FavoritesPage final : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesPage(QWidget *parent = nullptr);

public slots:
    void showLoading();
    void showError(const QString &message);
    void setStations(const QJsonArray &stations);
    void applyFavorite(qint64 stationId, bool favorited);

signals:
    void backRequested();
    void stationsRequested();
    void stationSelected(const QJsonObject &station);

private:
    void showState(const QString &text, bool retryVisible);
    void renderStations();

    QJsonArray m_stations;
    QListWidget *m_list = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_retryButton = nullptr;
};
