#include "repositories/stationrepository.h"

#include "database/databasemanager.h"

#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

bool StationRepository::listForUser(qint64 userId, bool favoritesOnly,
                                    QJsonArray *items, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT s.id, s.name, s.address, s.longitude, s.latitude, "
        "s.price_cents_per_kwh, COUNT(p.id), "
        "COALESCE(SUM(CASE WHEN p.status = 'idle' THEN 1 ELSE 0 END), 0), "
        "EXISTS(SELECT 1 FROM favorites f "
        "       WHERE f.user_id = ? AND f.station_id = s.id) "
        "FROM stations s "
        "LEFT JOIN charging_piles p ON p.station_id = s.id "
        "WHERE s.status = 'online' "
        "AND (? = 0 OR EXISTS(SELECT 1 FROM favorites ff "
        "                     WHERE ff.user_id = ? AND ff.station_id = s.id)) "
        "GROUP BY s.id, s.name, s.address, s.longitude, s.latitude, "
        "s.price_cents_per_kwh "
        "ORDER BY s.id"));
    query.addBindValue(userId);
    query.addBindValue(favoritesOnly ? 1 : 0);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }

    QJsonArray result;
    while (query.next()) {
        result.append(QJsonObject{
            {QStringLiteral("stationId"), query.value(0).toLongLong()},
            {QStringLiteral("name"), query.value(1).toString()},
            {QStringLiteral("address"), query.value(2).toString()},
            {QStringLiteral("longitude"), query.value(3).toDouble()},
            {QStringLiteral("latitude"), query.value(4).toDouble()},
            {QStringLiteral("priceCentsPerKwh"), query.value(5).toLongLong()},
            {QStringLiteral("totalPiles"), query.value(6).toInt()},
            {QStringLiteral("availablePiles"), query.value(7).toInt()},
            {QStringLiteral("favorited"), query.value(8).toBool()}
        });
    }
    if (items) *items = result;
    return true;
}

bool StationRepository::exists(qint64 stationId, bool *found, QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM stations WHERE id = ?"));
    query.addBindValue(stationId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (found) *found = query.next();
    return true;
}

bool StationRepository::setFavorite(qint64 userId, qint64 stationId,
                                    bool favorited, QString *updatedAt,
                                    QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;

    QSqlQuery query(db);
    if (favorited) {
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO favorites(user_id, station_id) VALUES(?, ?)"));
    } else {
        query.prepare(QStringLiteral(
            "DELETE FROM favorites WHERE user_id = ? AND station_id = ?"));
    }
    query.addBindValue(userId);
    query.addBindValue(stationId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }

    QSqlQuery timestamp(db);
    if (favorited) {
        timestamp.prepare(QStringLiteral(
            "SELECT created_at FROM favorites WHERE user_id = ? AND station_id = ?"));
        timestamp.addBindValue(userId);
        timestamp.addBindValue(stationId);
    } else {
        timestamp.prepare(QStringLiteral(
            "SELECT strftime('%Y-%m-%dT%H:%M:%fZ', 'now')"));
    }
    if (!timestamp.exec() || !timestamp.next()) {
        if (error) *error = timestamp.lastError().text();
        return false;
    }
    if (updatedAt) *updatedAt = timestamp.value(0).toString();
    return true;
}
