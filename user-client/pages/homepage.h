#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QComboBox;
class QListWidget;
class QPushButton;

class HomePage final : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    void setStations(const QJsonArray &stations);
    void showLoading();
    void showError(const QString &message);

public slots:
    void applyFavorite(qint64 stationId, bool favorited);
    void setResolvedLocation(double latitude, double longitude,
                             const QString &formattedAddress);
    void showAddressResolutionError(const QString &message);
    void useTextSearchFallback();
    double resolvedLatitude() const { return m_latitude; }
    double resolvedLongitude() const { return m_longitude; }
    bool hasResolvedLocation() const { return m_hasResolvedLocation; }
signals:
    void geocodeRequested(const QString &address, const QString &region);
    void stationsRequested(const QString &, const QString &, double, double);
    void stationSelected(const QJsonObject &);

private:
    void renderEmpty(const QString &text);
    void renderStations();
    void setLocationControlsBusy(bool busy);
    QString selectedRegion() const;
    QString currentLocationQuery() const;
    void requestStationsFromResolvedLocation();
    QLabel *m_status = nullptr;
    QLineEdit *m_address = nullptr;
    QComboBox *m_region = nullptr;
    QPushButton *m_gpsButton = nullptr;
    QPushButton *m_searchButton = nullptr;
    QListWidget *m_list = nullptr;
    QJsonArray m_stations;
    QString m_resolvedQuery;
    double m_latitude = 0.0;
    double m_longitude = 0.0;
    bool m_hasResolvedLocation = false;
};
