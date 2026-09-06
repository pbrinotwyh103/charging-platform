#pragma once

#include "protocol/errorcodes.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <functional>

struct MapGeocodeResult
{
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    QJsonObject coordinate;
    QString formattedAddress;

    bool succeeded() const { return error == Charging::ErrorCode::Success; }
};

class TencentMapAdapter final : public QObject
{
public:
    using GeocodeCallback = std::function<void(const MapGeocodeResult &)>;

    explicit TencentMapAdapter(QObject *parent = nullptr);

    void reloadConfiguration();
    bool isConfigured() const;
    void geocode(const QString &address, const QString &region,
                 GeocodeCallback callback);

    static QUrl buildGeocodeUrl(const QString &address, const QString &region,
                                const QString &apiKey);
    static MapGeocodeResult parseGeocodeResponse(const QByteArray &body,
                                                  const QString &address,
                                                  const QString &region);

private:
    void startGeocodeRequest(const QString &address, const QString &region,
                             int attempt, GeocodeCallback callback);

    QNetworkAccessManager m_network;
    QString m_apiKey;
};
