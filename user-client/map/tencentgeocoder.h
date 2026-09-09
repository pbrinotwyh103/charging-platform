#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

class TencentGeocoder final : public QObject
{
    Q_OBJECT

public:
    explicit TencentGeocoder(QObject *parent = nullptr);

    void lookup(const QString &address, const QString &region = {});

    static QUrl requestUrl(const QString &address, const QString &region,
                           const QString &key);
    static bool parseResponse(const QByteArray &data, double *latitude,
                              double *longitude, QString *error = nullptr);

signals:
    void resolved(double latitude, double longitude);
    void failed(const QString &message);

private:
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
};
