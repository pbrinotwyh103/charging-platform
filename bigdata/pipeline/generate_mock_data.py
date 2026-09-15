"""依据第一阶段 SQLite 表结构生成带有场景化质量问题的模拟数据。"""

from __future__ import annotations

import argparse
import csv
import json
import random
import sqlite3
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

from common import RAW_ROOT, SOURCE_DB, SOURCE_SNAPSHOT_ROOT, ensure_local_directories, write_json


RANDOM_SEED = 20260915
SHENZHEN_DISTRICTS = [
    "南山区",
    "福田区",
    "宝安区",
    "龙岗区",
    "龙华区",
    "罗湖区",
    "光明区",
    "坪山区",
]


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        raise ValueError(f"没有可写入的数据: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def fetch_rows(conn: sqlite3.Connection, sql: str) -> list[dict]:
    conn.row_factory = sqlite3.Row
    return [dict(row) for row in conn.execute(sql)]


def read_source_csv(filename: str) -> list[dict[str, Any]]:
    path = SOURCE_SNAPSHOT_ROOT / filename
    if not path.exists():
        raise FileNotFoundError(f"缺少第一阶段兼容快照: {path}")
    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def row_id(table: str, index: int) -> str:
    return f"{table}-{index:07d}"


def build_users(conn: sqlite3.Connection | None, rng: random.Random, count: int) -> list[dict]:
    source = (
        fetch_rows(conn, "SELECT id, phone, nickname, balance_cents, status, created_at FROM users ORDER BY id")
        if conn is not None
        else read_source_csv("phase1_users.csv")
    )
    rows: list[dict] = []
    for item in source:
        rows.append(
            {
                "_row_id": row_id("users", len(rows) + 1),
                "id": item["id"],
                "phone": item["phone"],
                "nickname": item["nickname"],
                "balance_cents": item["balance_cents"],
                "status": item["status"],
                "created_at": item["created_at"],
            }
        )

    start_id = max([int(row["id"]) for row in rows], default=0) + 1
    base_date = datetime(2026, 1, 1, tzinfo=timezone.utc)
    for offset in range(max(0, count - len(rows))):
        user_id = start_id + offset
        rows.append(
            {
                "_row_id": row_id("users", len(rows) + 1),
                "id": user_id,
                "phone": f"1{rng.choice([3, 5, 6, 7, 8, 9])}{rng.randrange(10**9):09d}",
                "nickname": f"深圳车主{user_id:04d}",
                "balance_cents": rng.randrange(0, 50001),
                "status": "frozen" if rng.random() < 0.025 else "normal",
                "created_at": (base_date + timedelta(days=rng.randrange(240))).isoformat(),
            }
        )

    # 场景化质量问题：空手机号、格式错误、负余额、非法状态、业务主键重复。
    rows[4]["phone"] = ""
    rows[11]["phone"] = "12345"
    rows[18]["balance_cents"] = -5000
    rows[27]["status"] = "deleted"
    duplicate = dict(rows[35])
    duplicate["_row_id"] = row_id("users", len(rows) + 1)
    rows.append(duplicate)
    return rows


def build_stations(conn: sqlite3.Connection | None, rng: random.Random, count: int) -> list[dict]:
    source = (
        fetch_rows(
            conn,
            f"""
        SELECT s.id, s.name, s.address, s.longitude, s.latitude,
               s.price_cents_per_kwh, s.status, s.created_at,
               u.source_id, u.raw_json
        FROM stations s
        JOIN urbanev_stations u ON u.station_id = s.id
        ORDER BY CAST(u.source_id AS INTEGER)
        LIMIT {int(count)}
        """,
        )
        if conn is not None
        else read_source_csv("urbanev_stations.csv")[:count]
    )
    selected = source[:count]
    rows: list[dict] = []
    for index, item in enumerate(selected, start=1):
        raw = json.loads(item["raw_json"])
        district = SHENZHEN_DISTRICTS[(index - 1) % len(SHENZHEN_DISTRICTS)]
        rows.append(
            {
                "_row_id": row_id("stations", index),
                "id": item["id"],
                "name": f"深圳-{int(item['id']):04d}",
                "address": item["address"] or f"深圳市{district}示范路{index}号",
                "district": district,
                "longitude": item["longitude"],
                "latitude": item["latitude"],
                "price_cents_per_kwh": item["price_cents_per_kwh"],
                "status": item["status"],
                "created_at": item["created_at"],
                "urbanev_source_id": item["source_id"],
                "urbanev_taz_id": raw.get("TAZID", ""),
                "source_dataset": "UrbanEV",
            }
        )

    if len(rows) < count:
        start_id = max([int(row["id"]) for row in rows], default=0) + 1
        for offset in range(count - len(rows)):
            station_id = start_id + offset
            district = SHENZHEN_DISTRICTS[offset % len(SHENZHEN_DISTRICTS)]
            rows.append(
                {
                    "_row_id": row_id("stations", len(rows) + 1),
                    "id": station_id,
                    "name": f"深圳-{station_id:04d}",
                    "address": f"深圳市{district}充电示范路{offset + 1}号",
                    "district": district,
                    "longitude": round(113.75 + rng.random() * 0.45, 6),
                    "latitude": round(22.45 + rng.random() * 0.35, 6),
                    "price_cents_per_kwh": rng.choice([120, 128, 135, 145, 150, 168]),
                    "status": "online" if rng.random() > 0.03 else "offline",
                    "created_at": datetime(2026, 1, 1, tzinfo=timezone.utc).isoformat(),
                    "urbanev_source_id": "",
                    "urbanev_taz_id": "",
                    "source_dataset": "synthetic_fallback",
                }
            )

    rows[3]["name"] = ""
    rows[8]["longitude"] = 999
    rows[13]["latitude"] = -999
    rows[21]["price_cents_per_kwh"] = -120
    rows[29]["status"] = "maintenance"
    duplicate = dict(rows[34])
    duplicate["_row_id"] = row_id("stations", len(rows) + 1)
    rows.append(duplicate)
    return rows


def build_piles(
    conn: sqlite3.Connection | None,
    rng: random.Random,
    stations: list[dict],
    count: int,
) -> list[dict]:
    station_ids = [int(row["id"]) for row in stations if str(row["id"]).isdigit()]
    source = (
        fetch_rows(
            conn,
            f"""
        SELECT p.id, p.station_id, p.pile_code, p.charge_type, p.power_kw,
               p.status, p.total_charge_count, p.total_charge_seconds,
               p.last_heartbeat_at, u.source_id, u.raw_json
        FROM charging_piles p
        JOIN urbanev_piles u ON u.pile_id = p.id
        ORDER BY CAST(u.source_id AS INTEGER)
        LIMIT {int(max(count * 2, count))}
        """,
        )
        if conn is not None
        else read_source_csv("urbanev_piles.csv")[: max(count * 2, count)]
    )
    rows: list[dict] = []
    valid_station_ids = set(station_ids)
    for item in source:
        if int(item["station_id"]) not in valid_station_ids or len(rows) >= count:
            continue
        raw = json.loads(item["raw_json"])
        rows.append(
            {
                "_row_id": row_id("piles", len(rows) + 1),
                "id": item["id"],
                "station_id": item["station_id"],
                "pile_code": item["pile_code"],
                "charge_type": item["charge_type"],
                "power_kw": item["power_kw"],
                "status": item["status"],
                "total_charge_count": item["total_charge_count"],
                "total_charge_seconds": item["total_charge_seconds"],
                "last_heartbeat_at": item["last_heartbeat_at"] or datetime.now(timezone.utc).isoformat(),
                "urbanev_source_id": item["source_id"],
                "urbanev_pile_type": raw.get("pileType", ""),
                "source_dataset": "UrbanEV",
            }
        )

    next_id = max([int(row["id"]) for row in rows], default=0) + 1
    while len(rows) < count:
        pile_id = next_id + len(rows)
        station_id = rng.choice(station_ids)
        charge_type = "fast" if rng.random() < 0.72 else "slow"
        power = rng.choice([60.0, 80.0, 120.0, 160.0]) if charge_type == "fast" else 7.0
        rows.append(
            {
                "_row_id": row_id("piles", len(rows) + 1),
                "id": pile_id,
                "station_id": station_id,
                "pile_code": f"SZ-{station_id:04d}-{pile_id:06d}",
                "charge_type": charge_type,
                "power_kw": power,
                "status": rng.choices(
                    ["idle", "charging", "reserved", "fault", "offline"],
                    weights=[60, 20, 8, 5, 7],
                )[0],
                "total_charge_count": rng.randrange(0, 3000),
                "total_charge_seconds": rng.randrange(0, 20_000_000),
                "last_heartbeat_at": (
                    datetime.now(timezone.utc) - timedelta(minutes=rng.randrange(180))
                ).isoformat(),
                "urbanev_source_id": "",
                "urbanev_pile_type": "DC" if charge_type == "fast" else "AC",
                "source_dataset": "synthetic_fallback",
            }
        )

    rows[6]["station_id"] = 999999
    rows[15]["pile_code"] = rows[14]["pile_code"]
    rows[24]["power_kw"] = 0
    rows[32]["charge_type"] = "super"
    rows[41]["status"] = "unknown"
    rows[49]["last_heartbeat_at"] = "not-a-time"
    return rows


def build_weather(rng: random.Random, start: datetime, days: int) -> list[dict]:
    rows: list[dict] = []
    for day_offset in range(days + 2):
        day = (start + timedelta(days=day_offset)).date()
        rainfall = 0.0 if rng.random() < 0.62 else round(rng.uniform(0.5, 45.0), 1)
        rows.append(
            {
                "_row_id": row_id("weather", day_offset + 1),
                "date": day.isoformat(),
                "temperature_c": round(rng.uniform(18.0, 34.0), 1),
                "rainfall_mm": rainfall,
                "weather": "雨" if rainfall > 0 else rng.choice(["晴", "多云", "阴"]),
                "is_holiday": 1 if day.weekday() >= 5 or rng.random() < 0.04 else 0,
            }
        )
    rows[7]["temperature_c"] = 88
    rows[19]["rainfall_mm"] = -10
    duplicate = dict(rows[25])
    duplicate["_row_id"] = row_id("weather", len(rows) + 1)
    rows.append(duplicate)
    return rows


def build_orders(
    rng: random.Random,
    users: list[dict],
    stations: list[dict],
    piles: list[dict],
    start: datetime,
    days: int,
    count: int,
) -> list[dict]:
    user_ids = [int(row["id"]) for row in users if str(row["id"]).isdigit()]
    station_lookup = {
        int(row["id"]): row
        for row in stations
        if str(row["id"]).isdigit() and float(row.get("price_cents_per_kwh") or 0) > 0
    }
    valid_piles = [row for row in piles if int(row.get("station_id") or -1) in station_lookup]
    rows: list[dict] = []

    for index in range(1, count + 1):
        pile = rng.choice(valid_piles)
        station = station_lookup[int(pile["station_id"])]
        day_offset = rng.randrange(days)
        hour = rng.choices(
            range(24),
            weights=[2, 1, 1, 1, 1, 2, 5, 9, 12, 10, 7, 6, 7, 8, 8, 7, 8, 12, 14, 12, 9, 6, 4, 3],
        )[0]
        started = start + timedelta(
            days=day_offset,
            hours=hour,
            minutes=rng.randrange(60),
            seconds=rng.randrange(60),
        )
        duration_seconds = rng.randrange(900, 9000)
        stopped = started + timedelta(seconds=duration_seconds)
        energy_wh = int(max(1500, rng.gauss(26000, 10500)))
        unit_price = int(station["price_cents_per_kwh"])
        fee_cents = int(round(energy_wh / 1000 * unit_price))
        status = rng.choices(
            ["completed", "fault_stopped", "cancelled"], weights=[94, 4, 2]
        )[0]
        if status == "cancelled":
            duration_seconds = 0
            energy_wh = 0
            fee_cents = 0
            stopped = started
        rows.append(
            {
                "_row_id": row_id("orders", index),
                "id": index,
                "order_no": f"BD-{started:%Y%m%d}-{index:08d}",
                "user_id": rng.choice(user_ids),
                "station_id": int(pile["station_id"]),
                "pile_id": int(pile["id"]),
                "status": status,
                "started_at": started.isoformat(),
                "stopped_at": stopped.isoformat(),
                "duration_seconds": duration_seconds,
                "energy_wh": energy_wh,
                "unit_price_cents": unit_price,
                "fee_cents": fee_cents,
                "stop_reason": "normal" if status == "completed" else status,
            }
        )

    rows[9]["order_no"] = rows[8]["order_no"]
    rows[39]["user_id"] = 999999
    rows[79]["pile_id"] = 99999999
    rows[119]["started_at"] = ""
    rows[159]["stopped_at"] = rows[159]["started_at"]
    rows[159]["duration_seconds"] = 3600
    rows[199]["duration_seconds"] = -300
    rows[239]["energy_wh"] = -12000
    rows[279]["fee_cents"] = -100
    rows[319]["status"] = "paid"
    rows[359]["fee_cents"] = int(rows[359]["fee_cents"]) + 9999
    rows[399]["energy_wh"] = 5_000_000
    duplicate = dict(rows[499])
    duplicate["_row_id"] = row_id("orders", len(rows) + 1)
    rows.append(duplicate)
    return rows


def build_alarms(
    rng: random.Random, piles: list[dict], orders: list[dict], count: int
) -> list[dict]:
    pile_ids = [int(row["id"]) for row in piles if str(row["id"]).isdigit()]
    order_ids = [int(row["id"]) for row in orders if str(row["id"]).isdigit()]
    kinds = [
        ("communication_timeout", "通信超时"),
        ("over_temperature", "设备温度过高"),
        ("voltage_anomaly", "输出电压异常"),
        ("emergency_stop", "急停按钮触发"),
    ]
    rows: list[dict] = []
    now = datetime.now(timezone.utc)
    for index in range(1, count + 1):
        alarm_type, message = rng.choice(kinds)
        occurred = now - timedelta(days=rng.randrange(90), hours=rng.randrange(24))
        status = rng.choices(["open", "acknowledged", "resolved"], [20, 25, 55])[0]
        rows.append(
            {
                "_row_id": row_id("alarms", index),
                "id": index,
                "pile_id": rng.choice(pile_ids),
                "order_id": rng.choice(order_ids) if rng.random() < 0.45 else "",
                "alarm_type": alarm_type,
                "severity": rng.choices(["info", "warning", "critical"], [20, 60, 20])[0],
                "message": message,
                "status": status,
                "occurred_at": occurred.isoformat(),
                "recovered_at": (
                    (occurred + timedelta(minutes=rng.randrange(5, 600))).isoformat()
                    if status == "resolved"
                    else ""
                ),
            }
        )
    rows[5]["pile_id"] = 99999999
    rows[12]["severity"] = "fatal"
    rows[20]["alarm_type"] = ""
    rows[28]["occurred_at"] = "bad-time"
    return rows


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-db", type=Path, default=SOURCE_DB)
    parser.add_argument("--users", type=int, default=1000)
    parser.add_argument("--stations", type=int, default=300)
    parser.add_argument("--piles", type=int, default=3000)
    parser.add_argument("--orders", type=int, default=100000)
    parser.add_argument("--days", type=int, default=90)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    ensure_local_directories()
    rng = random.Random(RANDOM_SEED)
    conn: sqlite3.Connection | None = None
    source_mode = "CSV compatibility snapshot"
    if args.source_db.exists():
        try:
            conn = sqlite3.connect(args.source_db)
            conn.execute("SELECT 1 FROM users LIMIT 1").fetchone()
            source_mode = "phase-one SQLite database"
        except sqlite3.DatabaseError:
            if conn is not None:
                conn.close()
            conn = None

    try:
        users = build_users(conn, rng, args.users)
        stations = build_stations(conn, rng, args.stations)
        piles = build_piles(conn, rng, stations, args.piles)
    finally:
        if conn is not None:
            conn.close()

    start = datetime.now(timezone.utc).replace(hour=0, minute=0, second=0, microsecond=0)
    start -= timedelta(days=args.days)
    weather = build_weather(rng, start, args.days)
    orders = build_orders(rng, users, stations, piles, start, args.days, args.orders)
    alarms = build_alarms(rng, piles, orders, max(240, args.orders // 80))

    datasets = {
        "users": users,
        "stations": stations,
        "charging_piles": piles,
        "charging_orders": orders,
        "alarms": alarms,
        "weather_daily": weather,
    }
    for name, rows in datasets.items():
        write_csv(RAW_ROOT / f"{name}.csv", rows)

    manifest = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "random_seed": RANDOM_SEED,
        "source_database": str(args.source_db),
        "source_mode": source_mode,
        "station_and_pile_source": "UrbanEV tables imported during phase one",
        "tables": {name: len(rows) for name, rows in datasets.items()},
        "quality_scenarios": [
            "空值与必填字段缺失",
            "业务主键重复",
            "手机号与时间格式错误",
            "经纬度、金额、功率等数值越界",
            "用户、站点、充电桩外键孤儿",
            "订单结束时间早于或等于开始时间",
            "充电金额与电量乘单价不一致",
            "能耗极端异常值",
        ],
    }
    write_json(RAW_ROOT / "manifest.json", manifest)
    print(f"模拟数据已生成: {RAW_ROOT}")
    for name, rows in datasets.items():
        print(f"  {name}: {len(rows)} 行")


if __name__ == "__main__":
    main()
