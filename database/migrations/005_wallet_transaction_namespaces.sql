-- Transaction numbers are unique inside the server-controlled record type.
-- Preserve the original numbers, IDs, amounts and timestamps on legacy ledgers.
CREATE TABLE wallet_records_v5 (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    record_no TEXT NOT NULL,
    user_id INTEGER NOT NULL REFERENCES users(id),
    order_id INTEGER REFERENCES charging_orders(id),
    record_type TEXT NOT NULL CHECK (record_type IN ('recharge', 'charge_payment', 'refund')),
    amount_cents INTEGER NOT NULL,
    balance_after_cents INTEGER NOT NULL CHECK (balance_after_cents >= 0),
    status TEXT NOT NULL DEFAULT 'success',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (record_type, record_no)
);

INSERT INTO wallet_records_v5
    SELECT id,record_no,user_id,order_id,record_type,amount_cents,balance_after_cents,status,created_at
    FROM wallet_records;

UPDATE sqlite_sequence SET seq=MAX(seq,COALESCE(
    (SELECT seq FROM sqlite_sequence WHERE name='wallet_records'),0))
    WHERE name='wallet_records_v5';

DROP TABLE wallet_records;
ALTER TABLE wallet_records_v5 RENAME TO wallet_records;

CREATE INDEX idx_wallet_user_created ON wallet_records(user_id, created_at DESC);
