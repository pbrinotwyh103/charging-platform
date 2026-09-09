#include "map/tencentgeocoder.h"

#include "map/mapnavigator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

TencentGeocoder::TencentGeocoder(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
}

QUrl TencentGeocoder::requestUrl(const QString &address, const QString &region,
                                 const QString &key)
{
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    if (!region.trimmed().isEmpty() && region != QStringLiteral("全部区域"))
        query.addQueryItem(QStringLiteral("region"), region.trimmed());
    query.addQueryItem(QStringLiteral("key"), key.trimmed());
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    url.setQuery(query);
    return url;
}

bool TencentGeocoder::parseResponse(const QByteArray &data, double *latitude,
                                    double *longitude, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("地图服务返回了无效数据");
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("status")).toInt(-1) != 0) {
        if (error) {
            const QString message = root.value(QStringLiteral("message")).toString().trimmed();
            *error = message.isEmpty() ? QStringLiteral("腾讯地图无法解析该地址") : message;
        }
        return false;
    }

    const QJsonObject location = root.value(QStringLiteral("result"))
                                     .toObject()
                                     .value(QStringLiteral("location"))
                                     .toObject();
    const QJsonValue latValue = location.value(QStringLiteral("lat"));
    const QJsonValue lngValue = location.value(QStringLiteral("lng"));
    if (!latValue.isDouble() || !lngValue.isDouble()) {
        if (error) *error = QStringLiteral("腾讯地图未返回地址坐标");
        return false;
    }

    const double lat = latValue.toDouble();
    const double lng = lngValue.toDouble();
    if (lat < -90 || lat > 90 || lng < -180 || lng > 180) {
        if (error) *error = QStringLiteral("腾讯地图返回了无效坐标");
        return false;
    }
    if (latitude) *latitude = lat;
    if (longitude) *longitude = lng;
    return true;
}

void TencentGeocoder::cancel()
{
    m_activeRequestId = 0;
    if (!m_reply) return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->abort();
    reply->deleteLater();
}

quint64 TencentGeocoder::lookup(const QString &address, const QString &region)
{
    cancel();
    const quint64 requestId = ++m_nextRequestId;
    m_activeRequestId = requestId;
    const QString normalizedAddress = address.trimmed();
    emit started(requestId, normalizedAddress);
    if (normalizedAddress.isEmpty()) {
        m_activeRequestId = 0;
        emit failed(requestId, QStringLiteral("请输入要搜索的地址"));
        return requestId;
    }
    const QString key = MapNavigator::apiKey();
    if (key.isEmpty()) {
        m_activeRequestId = 0;
        emit failed(requestId, QStringLiteral("地址搜索需要配置腾讯地图 Key（TENCENT_MAP_KEY 或 config/app.ini）"));
        return requestId;
    }

    QNetworkRequest request(requestUrl(normalizedAddress, region, key));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("charging-platform-user-client/1.0"));
    request.setTransferTimeout(10000);
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId] {
        if (m_reply != reply || m_activeRequestId != requestId) {
            reply->deleteLater();
            return;
        }
        m_reply = nullptr;
        m_activeRequestId = 0;

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                reply->deleteLater();
                return;
            }
            const QString message = reply->errorString();
            reply->deleteLater();
            emit failed(requestId, QStringLiteral("腾讯地图地址解析失败：%1").arg(message));
            return;
        }

        double latitude = 0;
        double longitude = 0;
        QString error;
        const bool ok = parseResponse(reply->readAll(), &latitude, &longitude, &error);
        reply->deleteLater();
        if (!ok) {
            emit failed(requestId, QStringLiteral("地址解析失败：%1").arg(error));
            return;
        }
        emit resolved(requestId, latitude, longitude);
    });
    return requestId;
}
