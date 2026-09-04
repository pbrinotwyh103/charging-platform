PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    phone TEXT NOT NULL UNIQUE,
    nickname TEXT NOT NULL,
    avatar_path TEXT,
    balance_cents INTEGER NOT NULL DEFAULT 0 CHECK (balance_cents >= 0),
    status TEXT NOT NULL DEFAULT 'normal' CHECK (status IN ('normal', 'frozen')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS admins (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'normal' CHECK (status IN ('normal', 'disabled')),
    last_login_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS stations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    longitude REAL NOT NULL,
    latitude REAL NOT NULL,
    price_cents_per_kwh INTEGER NOT NULL CHECK (price_cents_per_kwh >= 0),
    status TEXT NOT NULL DEFAULT 'online' CHECK (status IN ('online', 'offline')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS charging_piles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id INTEGER NOT NULL REFERENCES stations(id),
    pile_code TEXT NOT NULL UNIQUE,
    charge_type TEXT NOT NULL CHECK (charge_type IN ('fast', 'slow')),
    power_kw REAL NOT NULL CHECK (power_kw > 0),
    status TEXT NOT NULL DEFAULT 'idle'
        CHECK (status IN ('idle', 'reserved', 'charging', 'fault', 'offline', 'disabled')),
    total_charge_count INTEGER NOT NULL DEFAULT 0,
    total_charge_seconds INTEGER NOT NULL DEFAULT 0,
    last_heartbeat_at TEXT,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS favorites (
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    station_id INTEGER NOT NULL REFERENCES stations(id) ON DELETE CASCADE,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, station_id)
);

CREATE TABLE IF NOT EXISTS reservations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES users(id),
    pile_id INTEGER NOT NULL REFERENCES charging_piles(id),
    status TEXT NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'used', 'cancelled', 'expired')),
    reserved_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TEXT NOT NULL,
    used_at TEXT
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_reservation_per_pile
ON reservations(pile_id) WHERE status = 'active';

CREATE TABLE IF NOT EXISTS charging_orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    order_no TEXT NOT NULL UNIQUE,
    user_id INTEGER NOT NULL REFERENCES users(id),
    station_id INTEGER NOT NULL REFERENCES stations(id),
    pile_id INTEGER NOT NULL REFERENCES charging_piles(id),
    reservation_id INTEGER REFERENCES reservations(id),
    status TEXT NOT NULL CHECK (status IN ('charging', 'completed', 'fault_stopped', 'cancelled')),
    started_at TEXT NOT NULL,
    stopped_at TEXT,
    duration_seconds INTEGER NOT NULL DEFAULT 0,
    energy_wh INTEGER NOT NULL DEFAULT 0,
    unit_price_cents INTEGER NOT NULL,
    fee_cents INTEGER NOT NULL DEFAULT 0,
    stop_reason TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_one_charging_order_per_user
ON charging_orders(user_id) WHERE status = 'charging';

CREATE UNIQUE INDEX IF NOT EXISTS idx_one_charging_order_per_pile
ON charging_orders(pile_id) WHERE status = 'charging';

CREATE TABLE IF NOT EXISTS wallet_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    record_no TEXT NOT NULL UNIQUE,
    user_id INTEGER NOT NULL REFERENCES users(id),
    order_id INTEGER REFERENCES charging_orders(id),
    record_type TEXT NOT NULL CHECK (record_type IN ('recharge', 'charge_payment', 'refund')),
    amount_cents INTEGER NOT NULL,
    balance_after_cents INTEGER NOT NULL CHECK (balance_after_cents >= 0),
    status TEXT NOT NULL DEFAULT 'success',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS alarms (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    pile_id INTEGER REFERENCES charging_piles(id),
    order_id INTEGER REFERENCES charging_orders(id),
    alarm_type TEXT NOT NULL,
    severity TEXT NOT NULL CHECK (severity IN ('info', 'warning', 'critical')),
    message TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'acknowledged', 'resolved')),
    occurred_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    recovered_at TEXT,
    handled_by_admin_id INTEGER REFERENCES admins(id)
);

CREATE TABLE IF NOT EXISTS device_control_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admin_id INTEGER REFERENCES admins(id),
    pile_id INTEGER REFERENCES charging_piles(id),
    order_id INTEGER REFERENCES charging_orders(id),
    command_type TEXT NOT NULL,
    request_id INTEGER,
    result TEXT NOT NULL,
    detail TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS push_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_role TEXT NOT NULL CHECK (target_role IN ('user', 'administrator')),
    target_id INTEGER,
    message_type INTEGER NOT NULL,
    request_id INTEGER,
    result TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_piles_station_status ON charging_piles(station_id, status);
CREATE INDEX IF NOT EXISTS idx_orders_user_created ON charging_orders(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_orders_status ON charging_orders(status);
CREATE INDEX IF NOT EXISTS idx_alarms_status_time ON alarms(status, occurred_at DESC);

INSERT OR IGNORE INTO schema_version(version) VALUES (1);
