PRAGMA foreign_keys = ON;

-- 管理员账号在认证模块实现时通过带盐哈希初始化；禁止在数据库中写入明文密码。

INSERT OR IGNORE INTO stations(id, name, address, longitude, latitude, price_cents_per_kwh)
VALUES
    (1, '软件园充电站', '大连市高新园区软件园路', 121.5312, 38.8584, 120),
    (2, '星海充电站', '大连市沙河口区星海广场', 121.5868, 38.8817, 138);

INSERT OR IGNORE INTO charging_piles(id, station_id, pile_code, charge_type, power_kw, status)
VALUES
    (1, 1, 'DL-SP-001', 'fast', 60.0, 'idle'),
    (2, 1, 'DL-SP-002', 'slow', 7.0, 'idle'),
    (3, 2, 'DL-XH-001', 'fast', 120.0, 'idle');
