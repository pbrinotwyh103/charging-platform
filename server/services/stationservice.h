#pragma once
#include "protocol/errorcodes.h"
#include "services/servicebase.h"

#include <QJsonArray>
#include <QString>

struct StationListResult {
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    QJsonArray items;
};

struct FavoriteToggleResult {
    Charging::ErrorCode error = Charging::ErrorCode::Success;
    QString message;
    qint64 stationId = 0;
    bool favorited = false;
    QString updatedAt;
};

class StationService final : public ServiceBase
{
public:
    using ServiceBase::ServiceBase;

    StationListResult listForUser(qint64 userId, bool favoritesOnly);
    FavoriteToggleResult setFavorite(qint64 userId, qint64 stationId,
                                     bool favorited);
};
