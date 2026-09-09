#pragma once

#include <QWidget>
#include <QJsonArray>

class QLabel;
class QLineEdit;
class QComboBox;
class QListWidget;
class QPushButton;
class QBoxLayout;
class QVBoxLayout;

class HomePage final : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    void setStations(const QJsonArray &stations);
    void clearStations();
    void updateFavoriteState(qint64 stationId, bool favorited);
    void showLoading();
    void showError(const QString &message);
    void setSearchLocation(double latitude, double longitude);
    void showTextSearchFallback(const QString &message);
    void showGeocodingLoading(const QString &address);
    void showGeocodingSucceeded();
    void showGeocodingFailed(const QString &message);
    void clearGeocodingStatus();
    bool searchLocation(double *latitude, double *longitude) const;
    void setCompactLayout(bool compact);

signals:
    void stationsRequested(const QString &, const QString &, double, double);
    void stationSelected(const QJsonObject &);
    void geocodingRetryRequested();

private:
    void renderEmpty(const QString &text);
    QLabel *m_status = nullptr;
    QLabel *m_geocodingStatus = nullptr;
    QPushButton *m_geocodingRetryButton = nullptr;
    QPushButton *m_searchButton = nullptr;
    QLineEdit *m_address = nullptr;
    QComboBox *m_region = nullptr;
    QListWidget *m_list = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QBoxLayout *m_filterLayout = nullptr;
    double m_latitude = 22.5431;
    double m_longitude = 114.0579;
    bool m_simulatedLocationActive = false;
    bool m_hasSearchLocation = true;
    bool m_compactLayout = false;
};
