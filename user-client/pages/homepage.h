#pragma once

#include <QWidget>
#include <QJsonArray>

class QLabel;
class QLineEdit;
class QComboBox;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;

class HomePage final : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    void setStations(const QJsonArray &stations);
    void showLoading();
    void showError(const QString &message);
    void showPileLookupError(const QString &message);
signals:
    void stationsRequested(const QString &, const QString &, double, double);
    void stationSelected(const QJsonObject &);
    void pileCodeRequested(const QString &pileCode);

private:
    void renderEmpty(const QString &text);
    void requestStations();
    void cancelGeocoding();
    QLabel *m_status = nullptr;
    QLineEdit *m_address = nullptr;
    QLineEdit *m_pileCode = nullptr;
    QComboBox *m_region = nullptr;
    QListWidget *m_list = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_geocodingReply = nullptr;
    double m_latitude = 22.5431;
    double m_longitude = 114.0579;
    bool m_simulatedLocationActive = false;
};
