#pragma once

#include "database/databasemanager.h"

#include <QObject>
#include <QTemporaryDir>

class DatabaseRepositoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void schemaAndIntegrity();
    void upgradeLegacyChargingSequence();
    void profileStationAndFavoriteOperations();
    void reservationRulesAndExpiry();
    void chargingSettlementAndRollback();
    void rechargeIdempotency();
    void rechargeAndSettlementUseIndependentNamespaces();
    void legacyWalletNamespaceMigration();
    void atomicSettlementAndRecovery();
    void repositoryPagination();
    void faultSettlementAndLegacyReplay();
    void mixedFormatReservationExpiry();
    void isoExpiredReservationCannotStart();
    void repositoryUtcTimestamps();
    void mixedFormatPagination();
    void freshTimestampWrites_data();
    void freshTimestampWrites();
    void alarmControlAndPushRecords();
    void backupAndRestore();

private:
    qint64 createUser(const QString &phone);
    qint64 insertLegacyReservation(qint64 userId, const QString &expiresAt);

    QTemporaryDir m_temporaryDirectory;
    DatabaseManager m_database;
    QString m_databasePath;
};
