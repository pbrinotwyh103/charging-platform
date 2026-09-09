#pragma once

#include <QString>
#include <QUrl>

class MapNavigator final
{
public:
    static QString apiKey();
    static QUrl navigationUrl(double fromLat, double fromLon, double toLat,
                              double toLon, const QString &mode = QStringLiteral("driving"));
    static bool isConfigured();
    static bool isAllowedNavigationUrl(const QUrl &url);
};
