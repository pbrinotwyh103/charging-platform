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

    quint64 lookup(const QString &address, const QString &region = {});
    void cancel();
    quint64 activeRequestId() const { return m_activeRequestId; }

    static QUrl requestUrl(const QString &address, const QString &region,
                           const QString &key);
    static bool parseResponse(const QByteArray &data, double *latitude,
                              double *longitude, QString *error = nullptr);

signals:
    void started(quint64 requestId, const QString &address);
    void resolved(quint64 requestId, double latitude, double longitude);
    void failed(quint64 requestId, const QString &message);

private:
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    quint64 m_nextRequestId = 0;
    quint64 m_activeRequestId = 0;
};
