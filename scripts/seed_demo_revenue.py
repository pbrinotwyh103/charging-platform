#!/usr/bin/env python3
"""为管理端营收折线图生成可重复执行的近30日演示订单。"""

from __future__ import annotations

import argparse
import sqlite3
from datetime import date, datetime, time, timedelta, timezone
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="生成近期演示营收数据")
    parser.add_argument("database", type=Path, help="charging.db 路径")
    parser.add_argument("--days", type=int, default=30, help="生成天数，默认30")
    parser.add_argument(
        "--end-date", type=date.fromisoformat, default=date.today(),
        help="结束日期，格式 YYYY-MM-DD，默认今天",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if not 1 <= args.days <= 366:
        raise SystemExit("--days 必须在 1 到 366 之间")
    if not args.database.is_file():
        raise SystemExit(f"数据库不存在: {args.database}")

    connection = sqlite3.connect(args.database)
    connection.execute("PRAGMA foreign_keys = ON")
    users = [row[0] for row in connection.execute(
        "SELECT id FROM users WHERE status IN ('normal', 'active') ORDER BY id LIMIT 8"
    )]
    assets = connection.execute(
        """
        SELECT p.id, p.station_id, s.price_cents_per_kwh
          FROM charging_piles p
          JOIN stations s ON s.id = p.station_id
         WHERE s.status = 'online'
         ORDER BY p.id
         LIMIT 64
        """
    ).fetchall()
    if not users or not assets:
        raise SystemExit("至少需要一个启用用户和一个在线站点的电桩")

    rows = []
    first_day = args.end_date - timedelta(days=args.days - 1)
    for day_index in range(args.days):
        business_day = first_day + timedelta(days=day_index)
        # 工作日与周末使用不同单量，并叠加温和波动，便于展示趋势。
        order_count = 8 + (day_index * 5 % 5) + (2 if business_day.weekday() >= 5 else 0)
        for order_index in range(order_count):
            pile_id, station_id, unit_price = assets[
                (day_index * 7 + order_index * 3) % len(assets)
            ]
            user_id = users[(day_index + order_index) % len(users)]
            energy_wh = 22_000 + ((day_index * 3701 + order_index * 7919) % 43_000)
            fee_cents = round(energy_wh * unit_price / 1000)
            duration_seconds = 1_200 + energy_wh // 18
            started = datetime.combine(
                business_day,
                time(hour=7 + (order_index * 2) % 15, minute=(day_index * 11 + order_index * 7) % 60),
                timezone.utc,
            )
            stopped = started + timedelta(seconds=duration_seconds)
            order_no = f"DEMO-REV-{business_day:%Y%m%d}-{order_index + 1:02d}"
            rows.append((
                order_no, user_id, station_id, pile_id, "completed",
                started.isoformat().replace("+00:00", "Z"),
                stopped.isoformat().replace("+00:00", "Z"),
                duration_seconds, energy_wh, unit_price, fee_cents,
                "demo_completed",
                started.isoformat().replace("+00:00", "Z"),
                stopped.isoformat().replace("+00:00", "Z"),
            ))

    with connection:
        connection.execute("DELETE FROM charging_orders WHERE order_no LIKE 'DEMO-REV-%'")
        connection.executemany(
            """
            INSERT INTO charging_orders(
                order_no, user_id, station_id, pile_id, status,
                started_at, stopped_at, duration_seconds, energy_wh,
                unit_price_cents, fee_cents, stop_reason, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            rows,
        )
    total_cents = sum(row[10] for row in rows)
    print(
        f"已生成 {len(rows)} 笔演示订单，日期 {first_day} 至 {args.end_date}，"
        f"总营收 {total_cents / 100:.2f} 元"
    )


if __name__ == "__main__":
    main()
