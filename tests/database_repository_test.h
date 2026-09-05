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
    void profileStationAndFavoriteOperations();
    void reservationRulesAndExpiry();
    void chargingSettlementAndRollback();
    void alarmControlAndPushRecords();
    void backupAndRestore();

private:
    qint64 createUser(const QString &phone);

    QTemporaryDir m_temporaryDirectory;
    DatabaseManager m_database;
    QString m_databasePath;
};
