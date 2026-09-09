#pragma once

#include <QString>
#include <QUrl>

class QByteArray;

class MapNavigator final
{
public:
    static QString apiKey();
    static QUrl navigationUrl(double fromLat, double fromLon, double toLat,
                              double toLon, const QString &mode = QStringLiteral("driving"));
    static QUrl geocodingUrl(const QString &address, const QString &region = {});
    static bool parseGeocodingResponse(const QByteArray &body, double *latitude,
                                       double *longitude, QString *error = nullptr);
    static bool isConfigured();
};
