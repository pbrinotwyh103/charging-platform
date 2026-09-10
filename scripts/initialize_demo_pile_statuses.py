#!/usr/bin/env python3
"""为模拟定位附近的 UrbanEV 电桩生成可重复的演示运行状态。"""

import argparse
import hashlib
import json
import math
import sqlite3
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


def distance_km(latitude, longitude, station_latitude, station_longitude):
    radians = math.pi / 180.0
    delta_latitude = (station_latitude - latitude) * radians
    delta_longitude = (station_longitude - longitude) * radians
    value = (
        math.sin(delta_latitude / 2.0) ** 2
        + math.cos(latitude * radians)
        * math.cos(station_latitude * radians)
        * math.sin(delta_longitude / 2.0) ** 2
    )
    return 6371.0 * 2.0 * math.asin(math.sqrt(min(1.0, max(0.0, value))))


def stable_number(seed, purpose, pile_code):
    digest = hashlib.sha256(f"{seed}:{purpose}:{pile_code}".encode()).digest()
    return int.from_bytes(digest[:8], "big")


def chunks(values, size=500):
    for offset in range(0, len(values), size):
        yield values[offset : offset + size]


def backup_database(connection, database_path, backup_dir):
    backup_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup_path = backup_dir / f"{database_path.stem}.before-demo-status-{timestamp}.db"
    with sqlite3.connect(backup_path) as destination:
        connection.backup(destination)
    return backup_path


def run(database_path, latitude, longitude, radius_km, max_piles_per_station, seed, backup_dir):
    database_path = database_path.resolve(strict=True)
    connection = sqlite3.connect(database_path.as_uri() + "?mode=rw", uri=True, timeout=30)
    connection.execute("PRAGMA foreign_keys=ON")
    connection.row_factory = sqlite3.Row
    try:
        for table in ("stations", "charging_piles", "urbanev_stations", "urbanev_piles"):
            connection.execute(f"SELECT 1 FROM {table} LIMIT 0")
        if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("数据库完整性检查失败")

        piles_by_station = defaultdict(list)
        rows = connection.execute(
            """
            SELECT s.id AS station_id,s.latitude,s.longitude,p.id AS pile_id,p.pile_code,p.status
            FROM urbanev_stations us
            JOIN stations s ON s.id=us.station_id
            JOIN charging_piles p ON p.station_id=s.id
            JOIN urbanev_piles up ON up.pile_id=p.id
            WHERE s.status='online'
            """
        )
        for row in rows:
            if distance_km(latitude, longitude, row["latitude"], row["longitude"]) <= radius_km:
                piles_by_station[row["station_id"]].append(row)

        scoped_ids = [row["pile_id"] for piles in piles_by_station.values() for row in piles]
        if not scoped_ids:
            raise ValueError("指定范围内没有可初始化的 UrbanEV 电桩")

        protected = None
        for pile_ids in chunks(scoped_ids):
            placeholders = ",".join("?" for _ in pile_ids)
            protected = connection.execute(
                f"""
                SELECT p.id,p.status
                FROM charging_piles p
                WHERE p.id IN ({placeholders}) AND (
                    p.status IN ('reserved','disabled')
                    OR EXISTS (SELECT 1 FROM reservations r WHERE r.pile_id=p.id AND r.status='active')
                    OR EXISTS (SELECT 1 FROM charging_orders o WHERE o.pile_id=p.id AND o.status='charging')
                )
                LIMIT 1
                """,
                pile_ids,
            ).fetchone()
            if protected:
                break
        if protected:
            raise ValueError(
                f"电桩 {protected['id']} 当前状态为 {protected['status']}，存在活动业务，已拒绝覆盖"
            )

        assignments = {}
        for station_id, station_piles in piles_by_station.items():
            ordered = sorted(
                station_piles,
                key=lambda row: stable_number(seed, "select", row["pile_code"]),
            )
            selected = ordered[:max_piles_per_station]
            if not selected:
                continue
            # 每个有桩站点至少留一个可预约电桩。
            assignments[selected[0]["pile_id"]] = "idle"
            for row in selected[1:]:
                bucket = stable_number(seed, "status", row["pile_code"]) % 100
                assignments[row["pile_id"]] = (
                    "idle" if bucket < 70 else "charging" if bucket < 90 else "fault"
                )

        backup_path = backup_database(connection, database_path, backup_dir)
        now = datetime.now(timezone.utc).isoformat()
        counts = Counter(assignments.values())
        report = {
            "database": str(database_path),
            "backup": str(backup_path),
            "latitude": latitude,
            "longitude": longitude,
            "radiusKm": radius_km,
            "maxPilesPerStation": max_piles_per_station,
            "seed": seed,
            "nearbyStationsWithPiles": len(piles_by_station),
            "nearbyImportedPiles": len(scoped_ids),
            "selectedPiles": len(assignments),
            "statusCounts": dict(sorted(counts.items())),
            "note": "站点位置和额定功率来自 UrbanEV；电桩运行状态仅供演示，不代表实时状态。",
        }

        connection.execute("BEGIN IMMEDIATE")
        connection.executemany(
            "UPDATE charging_piles SET status='offline',updated_at=? WHERE id=?",
            ((now, pile_id) for pile_id in scoped_ids),
        )
        for status in ("idle", "charging", "fault"):
            pile_ids = [pile_id for pile_id, assigned in assignments.items() if assigned == status]
            connection.executemany(
                "UPDATE charging_piles SET status=?,updated_at=? WHERE id=?",
                ((status, now, pile_id) for pile_id in pile_ids),
            )
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS demo_pile_status_runs (
                id INTEGER PRIMARY KEY,
                applied_at TEXT NOT NULL,
                latitude REAL NOT NULL,
                longitude REAL NOT NULL,
                radius_km REAL NOT NULL,
                max_piles_per_station INTEGER NOT NULL,
                seed TEXT NOT NULL,
                report_json TEXT NOT NULL
            )
            """
        )
        connection.execute(
            """
            INSERT INTO demo_pile_status_runs(
                applied_at,latitude,longitude,radius_km,max_piles_per_station,seed,report_json
            ) VALUES(?,?,?,?,?,?,?)
            """,
            (now, latitude, longitude, radius_km, max_piles_per_station, seed, json.dumps(report)),
        )
        if connection.execute("PRAGMA foreign_key_check").fetchall():
            raise ValueError("更新后的数据库外键检查失败")
        connection.commit()
        checkpoint = connection.execute("PRAGMA wal_checkpoint(TRUNCATE)").fetchone()
        if checkpoint[0] != 0:
            raise RuntimeError("数据已提交，但 WAL 检查点被其他数据库连接阻塞")
        return report
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", type=Path, required=True, help="已有的 SQLite 数据库")
    parser.add_argument("--latitude", type=float, default=22.5431)
    parser.add_argument("--longitude", type=float, default=114.0579)
    parser.add_argument("--radius-km", type=float, default=10.0)
    parser.add_argument("--max-piles-per-station", type=int, default=10)
    parser.add_argument("--seed", default="urbanev-shenzhen-demo-v1")
    parser.add_argument("--backup-dir", type=Path, default=Path("database/backups"))
    args = parser.parse_args()
    if not -90 <= args.latitude <= 90 or not -180 <= args.longitude <= 180:
        parser.error("经纬度超出有效范围")
    if not 0 < args.radius_km <= 100:
        parser.error("搜索半径必须在 0 到 100 km 之间")
    if not 1 <= args.max_piles_per_station <= 100:
        parser.error("每站启用数量必须在 1 到 100 之间")
    result = run(
        args.db,
        args.latitude,
        args.longitude,
        args.radius_km,
        args.max_piles_per_station,
        args.seed,
        args.backup_dir.resolve(),
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
