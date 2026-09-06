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
private:
    qint64 createUser(const QString &phone);
    QTemporaryDir m_directory;
    DatabaseManager m_database;
};
