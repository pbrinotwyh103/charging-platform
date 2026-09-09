#!/usr/bin/env python3
"""将 UrbanEV 原始站点/电桩 CSV 导入现有 SQLite；不导入历史曲线或伪造实时状态。"""
import argparse
import csv
import hashlib
import json
import math
import sqlite3
from datetime import datetime, timezone
from decimal import Decimal
from pathlib import Path


def source_id(value):
    number = Decimal(value)
    if not number.is_finite() or number <= 0 or number != number.to_integral_value():
        raise ValueError('Invalid source id: ' + value)
    return str(int(number))


def demo_pile_status(pile_source_id):
    """为课程演示生成稳定的状态分布，不冒充数据集中的实时状态。"""
    bucket = int(hashlib.sha256(pile_source_id.encode('utf-8')).hexdigest()[:8], 16) % 100
    if bucket < 70:
        return 'idle'
    if bucket < 85:
        return 'charging'
    if bucket < 95:
        return 'offline'
    return 'fault'


def run(db_path, data_dir):
    stations_path = data_dir / 'station_information.csv'
    piles_path = data_dir / 'pile_rated_power.csv'
    with stations_path.open(encoding='utf-8-sig', newline='') as f:
        stations = list(csv.DictReader(f))
    with piles_path.open(encoding='utf-8-sig', newline='') as f:
        piles = list(csv.DictReader(f))
    station_ids = set()
    for row in stations:
        sid = source_id(row['station_id'])
        lon, lat = float(row['longitude']), float(row['latitude'])
        if sid in station_ids or not (-180 <= lon <= 180 and -90 <= lat <= 90):
            raise ValueError('Duplicate station or invalid coordinates: ' + sid)
        station_ids.add(sid)
    pile_ids = set()
    for row in piles:
        pid = source_id(row.get('pile_id') or row['pileNo'])
        if pid in pile_ids:
            raise ValueError('Duplicate pile: ' + pid)
        pile_ids.add(pid)

    # mode=rw 避免路径拼错时意外新建空数据库；备份保留原有账户、订单等数据。
    db_path = db_path.resolve(strict=True)
    con = sqlite3.connect(db_path.as_uri() + '?mode=rw', uri=True, timeout=30)
    con.execute('PRAGMA foreign_keys=ON')
    for table in ('stations', 'charging_piles'):
        con.execute('SELECT * FROM ' + table + ' LIMIT 0')
    backup = db_path.with_name(db_path.name + '.before-urbanev-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.bak')
    with sqlite3.connect(backup) as dest:
        con.backup(dest)
    report = dict(database=str(db_path), backup=str(backup), station_rows=len(stations),
                  pile_rows=len(piles), inserted_stations=0, inserted_piles=0,
                  quarantined_piles=0, skipped_stations=0, skipped_piles=0)
    provenance = {
        'paper': 'https://www.nature.com/articles/s41597-025-04874-4',
        'repository': 'https://github.com/IntelligentSystemsLab/UrbanEV',
        'download': 'https://drive.google.com/file/d/1OEpo-XDocd33aK3MbgDt9S5bcqwiAFPQ/view',
        'license': 'CC0-1.0', 'period': '2022-09-01/2023-02-28',
        'coordinate_system': 'GCJ-02 per latest author README; older Dryad metadata says WGS84; original values preserved without conversion',
        'station_name_address': 'Synthetic labels; real names and street addresses absent',
        'price': 'Dataset omits current tariff; stations.price_cents_per_kwh=150 is an explicit demo value, not an observed tariff',
        'status': 'Synthetic classroom demo state: about 70% idle, 15% charging, 10% offline and 5% fault; not observed realtime state',
        'pile_types': 'AC mapped to slow; DC mapped to fast',
        'csv_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (stations_path, piles_path)},
    }
    try:
        con.execute('BEGIN IMMEDIATE')
        con.execute('CREATE TABLE IF NOT EXISTS urbanev_stations (source_id TEXT PRIMARY KEY, station_id INTEGER NOT NULL UNIQUE REFERENCES stations(id), raw_json TEXT NOT NULL, current_price_known INTEGER NOT NULL DEFAULT 0)')
        con.execute('CREATE TABLE IF NOT EXISTS urbanev_piles (source_id TEXT PRIMARY KEY, pile_id INTEGER UNIQUE REFERENCES charging_piles(id), raw_json TEXT NOT NULL, rejection_reason TEXT)')
        con.execute('CREATE TABLE IF NOT EXISTS urbanev_import_runs (id INTEGER PRIMARY KEY, imported_at TEXT NOT NULL, provenance_json TEXT NOT NULL, report_json TEXT NOT NULL)')
        mapping = dict(con.execute('SELECT source_id,station_id FROM urbanev_stations'))
        for row in stations:
            sid = source_id(row['station_id'])
            if sid in mapping:
                report['skipped_stations'] += 1
                continue
            local_id = con.execute('INSERT INTO stations (name,address,longitude,latitude,price_cents_per_kwh,status) VALUES (?,?,?,?,?,?)',
                ('深圳-' + sid, '深圳市 · UrbanEV历史站点（详细地址未提供）',
                 float(row['longitude']), float(row['latitude']), 150, 'online')).lastrowid
            con.execute('INSERT INTO urbanev_stations(source_id,station_id,raw_json) VALUES (?,?,?)', (sid, local_id, json.dumps(row)))
            mapping[sid] = local_id
            report['inserted_stations'] += 1
        existing = {r[0] for r in con.execute('SELECT source_id FROM urbanev_piles')}
        for row in piles:
            pid = source_id(row.get('pile_id') or row['pileNo'])
            if pid in existing:
                report['skipped_piles'] += 1
                continue
            sid = source_id(row['station_id'])
            power = float(row['power'])
            reason = None
            if sid not in mapping:
                reason = 'Station missing'
            elif not math.isfinite(power) or power <= 0:
                reason = 'Missing/invalid rated power; do not invent a value'
            elif row['pileType'] not in ('AC', 'DC'):
                reason = 'Unknown pile type'
            local_id = None
            if reason:
                report['quarantined_piles'] += 1
            else:
                status = demo_pile_status(pid)
                local_id = con.execute('INSERT INTO charging_piles (station_id,pile_code,charge_type,power_kw,status) VALUES (?,?,?,?,?)',
                    (mapping[sid], 'URBANEV-' + pid, 'slow' if row['pileType'] == 'AC' else 'fast', power, status)).lastrowid
                report['inserted_piles'] += 1
            # 不合法记录仍完整保留，避免为了满足数据库约束随意补造功率。
            con.execute('INSERT INTO urbanev_piles VALUES (?,?,?,?)', (pid, local_id, json.dumps(row), reason))
        report['totals'] = {t: con.execute('SELECT COUNT(*) FROM ' + t).fetchone()[0] for t in ('stations', 'charging_piles', 'users', 'charging_orders')}
        if con.execute('PRAGMA foreign_key_check').fetchall():
            raise ValueError('Foreign key check failed')
        if con.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
            raise ValueError('Integrity check failed')
        con.execute('INSERT INTO urbanev_import_runs(imported_at,provenance_json,report_json) VALUES (?,?,?)',
                    (datetime.now(timezone.utc).isoformat(), json.dumps(provenance), json.dumps(report)))
        con.commit()
    except Exception:
        con.rollback()
        raise
    finally:
        con.close()
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--db', type=Path, required=True)
    parser.add_argument('--data-dir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = run(args.db, args.data_dir)
    except Exception as exc:
        args.report.write_text(json.dumps({'error': str(exc)}, ensure_ascii=False, indent=2), encoding='utf-8')
        raise
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))
