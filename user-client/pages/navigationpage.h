#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QWebEngineView;

class NavigationPage final : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);
    void setRoute(double fromLatitude, double fromLongitude,
                  double toLatitude, double toLongitude,
                  const QString &destinationName);

signals:
    void backRequested();

private:
    void loadRoute();
    void showFallback(const QString &message, bool canRetry);

    QComboBox *m_mode = nullptr;
    QLabel *m_destination = nullptr;
    QLabel *m_state = nullptr;
    QPushButton *m_retry = nullptr;
    QWebEngineView *m_view = nullptr;
    double m_fromLatitude = 0.0;
    double m_fromLongitude = 0.0;
    double m_toLatitude = 0.0;
    double m_toLongitude = 0.0;
    QString m_destinationName;
    bool m_showingFallback = false;
};
