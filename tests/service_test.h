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
    void avatarValidationAndPersistence();
    void avatarDatabaseFailureKeepsOldFile();
    void rechargeBoundsAndIdempotency();
    void walletPaginationAndOrdering();
    void nearbyTextSearchAndCoordinates();
    void pileStatusAndFavoriteIdempotency();
    void databaseFailures();
private:
    qint64 createUser(const QString &phone);
    QTemporaryDir m_directory;
    DatabaseManager m_database;
};
