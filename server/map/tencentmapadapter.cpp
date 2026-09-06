#include "map/tencentmapadapter.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>

#include <cmath>

namespace {

constexpr int kProviderTimeoutMs = 3'000;
constexpr int kMaximumResponseBytes = 1024 * 1024;

bool containsAny(const QString &text, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (text.contains(needle, Qt::CaseInsensitive)) return true;
    }
    return false;
}

MapGeocodeResult unavailableResult(const QString &message)
{
    MapGeocodeResult result;
    result.error = Charging::ErrorCode::NetworkUnavailable;
    result.message = message;
    return result;
}

} // namespace

TencentMapAdapter::TencentMapAdapter(QObject *parent)
    : QObject(parent), m_network(this)
{
    reloadConfiguration();
}

void TencentMapAdapter::reloadConfiguration()
{
    const QByteArray environmentKey = qgetenv("TENCENT_MAP_KEY");
    if (!environmentKey.isEmpty()) {
        m_apiKey = QString::fromUtf8(environmentKey).trimmed();
        return;
    }

    QSettings settings(
        QCoreApplication::applicationDirPath()
            + QStringLiteral("/../../config/app.ini"),
        QSettings::IniFormat);
    m_apiKey = settings.value(QStringLiteral("map/tencent_key"))
                   .toString().trimmed();
}

bool TencentMapAdapter::isConfigured() const
{
    return !m_apiKey.isEmpty();
}

QUrl TencentMapAdapter::buildGeocodeUrl(const QString &address,
                                        const QString &region,
                                        const QString &apiKey)
{
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    if (!region.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("region"), region.trimmed());
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("key"), apiKey.trimmed());
    url.setQuery(query);
    return url;
}

MapGeocodeResult TencentMapAdapter::parseGeocodeResponse(
    const QByteArray &body, const QString &address, const QString &region)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return unavailableResult(QStringLiteral("腾讯地图返回了无法解析的数据"));

    const QJsonObject root = document.object();
    const int providerStatus = root.value(QStringLiteral("status")).toInt(-1);
    const QString providerMessage =
        root.value(QStringLiteral("message")).toString().trimmed();
    if (providerStatus != 0) {
        MapGeocodeResult result;
        if (containsAny(providerMessage,
                        {QStringLiteral("配额"), QStringLiteral("限流"),
                         QStringLiteral("频率"), QStringLiteral("调用量"),
                         QStringLiteral("quota"), QStringLiteral("limit")})) {
            result.error = Charging::ErrorCode::RateLimited;
            result.message = QStringLiteral("腾讯地图调用额度已用尽或请求过于频繁，请稍后重试");
        } else if (containsAny(providerMessage,
                               {QStringLiteral("无结果"), QStringLiteral("未找到"),
                                QStringLiteral("not found"), QStringLiteral("no result")})) {
            result.error = Charging::ErrorCode::NotFound;
            result.message = QStringLiteral("腾讯地图未找到该地址，请补充更详细的地址");
        } else if (containsAny(providerMessage,
                               {QStringLiteral("key"), QStringLiteral("密钥"),
                                QStringLiteral("权限")})) {
            result.error = Charging::ErrorCode::NetworkUnavailable;
            result.message = QStringLiteral("腾讯地图密钥无效或未开通 WebService API");
        } else {
            result.error = Charging::ErrorCode::NetworkUnavailable;
            result.message = QStringLiteral("腾讯地图地址解析服务暂时不可用");
        }
        return result;
    }

    const QJsonObject providerResult =
        root.value(QStringLiteral("result")).toObject();
    const QJsonObject location =
        providerResult.value(QStringLiteral("location")).toObject();
    if (!location.value(QStringLiteral("lat")).isDouble()
        || !location.value(QStringLiteral("lng")).isDouble()) {
        MapGeocodeResult result;
        result.error = Charging::ErrorCode::NotFound;
        result.message = QStringLiteral("腾讯地图未找到该地址，请补充更详细的地址");
        return result;
    }

    const double latitude = location.value(QStringLiteral("lat")).toDouble();
    const double longitude = location.value(QStringLiteral("lng")).toDouble();
    if (!std::isfinite(latitude) || !std::isfinite(longitude)
        || latitude < -90.0 || latitude > 90.0
        || longitude < -180.0 || longitude > 180.0) {
        return unavailableResult(QStringLiteral("腾讯地图返回的坐标无效"));
    }

    MapGeocodeResult result;
    result.coordinate = {
        {QStringLiteral("lat"), latitude},
        {QStringLiteral("lng"), longitude},
        {QStringLiteral("crs"), QStringLiteral("GCJ-02")}
    };
    result.formattedAddress =
        providerResult.value(QStringLiteral("title")).toString().trimmed();
    if (result.formattedAddress.isEmpty()) {
        result.formattedAddress = region.trimmed() + address.trimmed();
    }
    return result;
}

void TencentMapAdapter::geocode(const QString &address, const QString &region,
                                GeocodeCallback callback)
{
    const QString normalizedAddress = address.trimmed();
    if (normalizedAddress.isEmpty() || normalizedAddress.size() > 200) {
        MapGeocodeResult result;
        result.error = Charging::ErrorCode::ValidationFailed;
        result.message = QStringLiteral("地址长度必须为 1—200 个字符");
        callback(result);
        return;
    }
    if (!isConfigured()) {
        callback(unavailableResult(
            QStringLiteral("腾讯地图密钥未配置，请设置 TENCENT_MAP_KEY 或 config/app.ini")));
        return;
    }
    startGeocodeRequest(normalizedAddress, region.trimmed(), 0,
                        std::move(callback));
}

void TencentMapAdapter::startGeocodeRequest(const QString &address,
                                            const QString &region, int attempt,
                                            GeocodeCallback callback)
{
    QNetworkRequest request(buildGeocodeUrl(address, region, m_apiKey));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("charging-platform/1.0"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(kProviderTimeoutMs);

    QNetworkReply *reply = m_network.get(request);
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply] {
        reply->setProperty("chargingTimedOut", true);
        reply->abort();
    });
    timer->start(kProviderTimeoutMs);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, timer, address, region, attempt,
             callback = std::move(callback)]() mutable {
        timer->stop();
        const bool timedOut = reply->property("chargingTimedOut").toBool();
        const int httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QByteArray body = reply->readAll();
        reply->deleteLater();

        const bool retryable = timedOut
            || networkError != QNetworkReply::NoError
            || httpStatus >= 500;
        if (retryable && attempt == 0) {
            startGeocodeRequest(address, region, 1, std::move(callback));
            return;
        }
        if (timedOut) {
            MapGeocodeResult result;
            result.error = Charging::ErrorCode::RequestTimeout;
            result.message = QStringLiteral("腾讯地图地址解析超时，请重试");
            callback(result);
            return;
        }
        if (httpStatus == 429) {
            MapGeocodeResult result;
            result.error = Charging::ErrorCode::RateLimited;
            result.message = QStringLiteral("腾讯地图请求过于频繁，请稍后重试");
            callback(result);
            return;
        }
        if (httpStatus == 404) {
            MapGeocodeResult result;
            result.error = Charging::ErrorCode::NotFound;
            result.message = QStringLiteral("腾讯地图未找到该地址");
            callback(result);
            return;
        }
        if (networkError != QNetworkReply::NoError
            || httpStatus < 200 || httpStatus >= 300) {
            callback(unavailableResult(
                QStringLiteral("无法连接腾讯地图地址解析服务，请稍后重试")));
            return;
        }
        if (body.size() > kMaximumResponseBytes) {
            callback(unavailableResult(QStringLiteral("腾讯地图响应数据异常")));
            return;
        }
        callback(parseGeocodeResponse(body, address, region));
    });
}
