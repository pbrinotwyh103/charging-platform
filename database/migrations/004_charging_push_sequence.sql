ALTER TABLE charging_orders ADD COLUMN push_seq INTEGER NOT NULL DEFAULT 0
    CHECK (push_seq >= 0 AND push_seq <= 9007199254740991);
