CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_reservation_per_user
ON reservations(user_id) WHERE status = 'active';

CREATE INDEX IF NOT EXISTS idx_reservations_user_status
ON reservations(user_id, status, expires_at);

CREATE INDEX IF NOT EXISTS idx_reservations_expiry
ON reservations(status, expires_at);

CREATE INDEX IF NOT EXISTS idx_favorites_station
ON favorites(station_id, user_id);

CREATE INDEX IF NOT EXISTS idx_orders_station_created
ON charging_orders(station_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_orders_pile_created
ON charging_orders(pile_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_wallet_user_created
ON wallet_records(user_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_controls_pile_created
ON device_control_records(pile_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_push_target_created
ON push_records(target_role, target_id, created_at DESC);
