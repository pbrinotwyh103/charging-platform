#pragma once

#include "services/serviceresult.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QUrl>
#include <functional>

struct AnalyticsFetchResult
{
    bool transportOk = false;
    int statusCode = 0;
    QByteArray body;
    QString error;
};

class AnalyticsService final
{
public:
    using Fetcher = std::function<AnalyticsFetchResult(const QUrl &)>;

    explicit AnalyticsService(Fetcher fetcher = {});

    ServiceResult predictions(const QJsonObject &payload);
    ServiceResult recommendations(const QJsonObject &payload);
    ServiceResult warnings(const QJsonObject &payload);
    ServiceResult status();
    ServiceResult report(const QString &name, const QJsonObject &payload = {});

private:
    ServiceResult predictionQuery(const QJsonObject &payload, const QString &mode);
    AnalyticsFetchResult fetch(const QUrl &url) const;
    QUrl endpoint(const QString &path) const;

    Fetcher m_fetcher;
    mutable QMutex m_cacheMutex;
    QHash<QString, QJsonObject> m_cache;
};
