#pragma once

#include "services/adminservice.h"
#include "services/alarmservice.h"
#include "services/authservice.h"
#include "services/billingservice.h"
#include "services/chargingservice.h"
#include "services/orderservice.h"
#include "services/pileservice.h"
#include "services/reservationservice.h"
#include "services/stationservice.h"
#include "services/statisticsservice.h"
#include "services/userservice.h"

#include <QObject>

class DatabaseManager;

class ServiceRegistry final : public QObject
{
    Q_OBJECT

public:
    explicit ServiceRegistry(QObject *parent = nullptr);
    bool initialize(DatabaseManager *database, QString *error);
    bool isInitialized() const;

    AuthService *auth() { return &m_auth; }
    UserService *users() { return &m_users; }
    StationService *stations() { return &m_stations; }
    PileService *piles() { return &m_piles; }
    ReservationService *reservations() { return &m_reservations; }
    ChargingService *charging() { return &m_charging; }
    BillingService *billing() { return &m_billing; }
    OrderService *orders() { return &m_orders; }
    AlarmService *alarms() { return &m_alarms; }
    StatisticsService *statistics() { return &m_statistics; }
    AdminService *admin() { return &m_admin; }

private:
    DatabaseManager *m_database = nullptr;
    AuthService m_auth;
    UserService m_users;
    StationService m_stations;
    PileService m_piles;
    ReservationService m_reservations;
    ChargingService m_charging;
    BillingService m_billing;
    OrderService m_orders;
    AlarmService m_alarms;
    StatisticsService m_statistics;
    AdminService m_admin;
};
