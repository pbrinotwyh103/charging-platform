#pragma once

#include <QJsonObject>
#include <QUrl>
#include <QWidget>

class QLabel;
class QPushButton;
class QWebEngineView;
class QVBoxLayout;

class NavigationPage final : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);
    void showRoute(const QJsonObject &station, const QString &mode,
                   double fromLatitude, double fromLongitude, bool hasOrigin);
    QUrl routeUrl() const { return m_routeUrl; }
    void setCompactLayout(bool compact);

signals:
    void backRequested();

private:
    void loadRoute();
    void showFallback();

    QJsonObject m_station;
    QString m_mode;
    QUrl m_routeUrl;
    QLabel *m_title = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_retry = nullptr;
    QWebEngineView *m_webView = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
};
