#pragma once

#include "database/databasemanager.h"
#include <QObject>
#include <QTemporaryDir>

class ServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void profileValidationAndPartialUpdate();
    void concurrentIndependentProfileUpdates();
    void profileResponseReflectsCommittedRow();
    void avatarValidationAndPersistence();
    void avatarDatabaseFailureKeepsOldFile();
    void rechargeBoundsAndIdempotency();
    void walletPaginationAndOrdering();
    void nearbyTextSearchAndCoordinates();
    void pileStatusAndFavoriteIdempotency();
    void databaseFailures();
    void reservationSelectionAndDuration();
    void reservationValidationAndConflicts();
    void reservationCancelAndExpiry();
    void reservationWriteFailureRollsBack();
    void activeOrderAndSettlement();
    void billingRoundingAndValidation();
    void settlementFailureRollsBack();
    void reservationTransactionRechecksEligibility();
    void expiryPreservesUnavailablePiles();
    void stationChangeAfterPrecheckRejectsReservation();
    void simultaneousReservations();
    void directOrderCannotBypassActiveReservation();
    void reservationContentionWaitsBeforeReading();
    void simultaneousDirectOrderAndReservation();
    void chargingProgressAndIdempotentStop();
    void chargingAutomaticStops();
    void chargingSettlementRetryAndAlarmDeduplication();
    void chargingRestoreAndPersistedProgress();
    void chargingValidationAndOwnership();
    void chargingStartRechecksEligibilityAtWrite();
    void chargingRestoreUnavailablePile();
    void chargingProgressFailureDoesNotPublish();
    void chargingSequenceSurvivesRestart();
    void chargingNormalStopRetryAfterPileFailure();
    void adminQueriesStatisticsAndExport();
    void adminMaintenanceAndAliases();
    void adminControlConflictsAndAudits();
    void adminRemoteStopAndFreeze();
    void adminValidationAndDatabaseFailures();
    void adminControlReplayAndClientMetadata();
    void adminAlarmDetailSurvivesListFilters();
    void adminControlReplayIsScopedToConnection();
    void adminStopRetryAfterAuditFailureKeepsOriginalOrder();
    void adminRestartRejectsEveryActiveReservation();
    void adminDeviceChangeRollsBackWithAudit();
    void adminRepositorySnapshotsSurviveConcurrentChanges();
    void adminStatisticsRejectsOverflow();
    void adminExportHonorsEncodedTransportLimit();
    void adminExportKeepsLargeIntegers();
private:
    qint64 createUser(const QString &phone);
    QTemporaryDir m_directory;
    DatabaseManager m_database;
};
