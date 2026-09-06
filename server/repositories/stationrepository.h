#pragma once
#include "repositories/repositorybase.h"

#include <QJsonArray>

class StationRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool listForUser(qint64 userId, bool favoritesOnly, QJsonArray *items,
                     QString *error) const;
    bool exists(qint64 stationId, bool *found, QString *error) const;
    bool setFavorite(qint64 userId, qint64 stationId, bool favorited,
                     QString *updatedAt, QString *error) const;
};
