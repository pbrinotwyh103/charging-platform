#pragma once

#include <QWidget>
#include <QJsonArray>

class QLabel;
class QLineEdit;
class QComboBox;
class QListWidget;

class HomePage final : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    void setStations(const QJsonArray &stations);
    void showLoading();
    void showError(const QString &message);
signals:
    void stationsRequested(const QString &, const QString &, double, double);
    void stationSelected(const QJsonObject &);

private:
    void renderEmpty(const QString &text);
    QLabel *m_status = nullptr;
    QLineEdit *m_address = nullptr;
    QComboBox *m_region = nullptr;
    QListWidget *m_list = nullptr;
    double m_latitude = 22.5431;
    double m_longitude = 114.0579;
    bool m_simulatedLocationActive = false;
};
