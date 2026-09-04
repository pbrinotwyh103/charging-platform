#include "services/serviceregistry.h"

#include "database/databasemanager.h"

ServiceRegistry::ServiceRegistry(QObject *parent)
    : QObject(parent)
{
}

bool ServiceRegistry::initialize(DatabaseManager *database, QString *error)
{
    m_database = database;
    m_auth.setDatabase(database);
    m_users.setDatabase(database);
    m_stations.setDatabase(database);
    m_piles.setDatabase(database);
    m_reservations.setDatabase(database);
    m_charging.setDatabase(database);
    m_billing.setDatabase(database);
    m_orders.setDatabase(database);
    m_alarms.setDatabase(database);
    m_statistics.setDatabase(database);
    m_admin.setDatabase(database);
    return m_auth.initialize(error);
}

bool ServiceRegistry::isInitialized() const
{
    return m_database != nullptr;
}
