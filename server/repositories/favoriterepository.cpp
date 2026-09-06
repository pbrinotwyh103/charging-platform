#include "repositories/favoriterepository.h"

#include "database/databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

bool FavoriteRepository::add(qint64 userId, qint64 stationId, bool *created,
                             QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO favorites(user_id,station_id) VALUES(?,?)"));
    query.addBindValue(userId);
    query.addBindValue(stationId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (created) *created = query.numRowsAffected() == 1;
    return true;
}

bool FavoriteRepository::remove(qint64 userId, qint64 stationId, bool *removed,
                                QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM favorites WHERE user_id=? AND station_id=?"));
    query.addBindValue(userId);
    query.addBindValue(stationId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (removed) *removed = query.numRowsAffected() == 1;
    return true;
}

bool FavoriteRepository::contains(qint64 userId, qint64 stationId, bool *favorite,
                                  QString *error) const
{
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM favorites WHERE user_id=? AND station_id=?"));
    query.addBindValue(userId);
    query.addBindValue(stationId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    *favorite = query.next();
    return true;
}

bool FavoriteRepository::listStationIds(qint64 userId, QList<qint64> *stationIds,
                                        QString *error) const
{
    stationIds->clear();
    QSqlDatabase db = database()->database(error);
    if (!db.isValid() || !db.isOpen()) return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT station_id FROM favorites WHERE user_id=? ORDER BY created_at DESC"));
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) stationIds->append(query.value(0).toLongLong());
    return true;
}
