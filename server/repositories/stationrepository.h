#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct StationRecord {
    qint64 id = 0;
    QString name;
    QString address;
    double longitude = 0.0;
    double latitude = 0.0;
    qint64 priceCentsPerKwh = 0;
    QString status;
    int totalPileCount = 0;
    int idlePileCount = 0;
    int onlinePileCount = 0;
    QString createdAt;
    QString updatedAt;
};

class StationRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool findById(qint64 stationId, StationRecord *record, QString *error) const;
    bool list(const QString &status, QList<StationRecord> *records, QString *error) const;
    bool insert(const StationRecord &record, qint64 *stationId, QString *error) const;
    bool update(const StationRecord &record, QString *error) const;
};
