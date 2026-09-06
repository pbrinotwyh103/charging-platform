#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

class FavoriteRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool add(qint64 userId, qint64 stationId, bool *created, QString *error) const;
    bool remove(qint64 userId, qint64 stationId, bool *removed, QString *error) const;
    bool contains(qint64 userId, qint64 stationId, bool *favorite, QString *error) const;
    bool listStationIds(qint64 userId, QList<qint64> *stationIds, QString *error) const;
};
