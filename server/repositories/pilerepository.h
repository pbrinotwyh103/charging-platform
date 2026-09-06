#pragma once

#include "repositories/repositorybase.h"

#include <QList>
#include <QString>

struct PileRecord {
    qint64 id = 0;
    qint64 stationId = 0;
    QString pileCode;
    QString chargeType;
    double powerKw = 0.0;
    QString status;
    int totalChargeCount = 0;
    qint64 totalChargeSeconds = 0;
    QString lastHeartbeatAt;
    QString updatedAt;
};

class PileRepository final : public RepositoryBase
{
public:
    using RepositoryBase::RepositoryBase;

    bool findById(qint64 pileId, PileRecord *record, QString *error) const;
    bool listByStation(qint64 stationId, const QString &status,
                       QList<PileRecord> *records, QString *error) const;
    bool insert(const PileRecord &record, qint64 *pileId, QString *error) const;
    bool updateStatus(qint64 pileId, const QString &expectedStatus,
                      const QString &newStatus, QString *error) const;
    bool updateHeartbeat(qint64 pileId, QString *error) const;
};
