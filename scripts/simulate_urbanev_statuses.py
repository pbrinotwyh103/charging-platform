#!/usr/bin/env python3
"""为已导入的 UrbanEV 电桩生成可复现的课堂演示状态。"""

import argparse
import hashlib
import shutil
import sqlite3
from datetime import datetime
from pathlib import Path


def demo_status(source_id):
    bucket = int(hashlib.sha256(source_id.encode('utf-8')).hexdigest()[:8], 16) % 100
    if bucket < 70:
        return 'idle'
    if bucket < 85:
        return 'charging'
    if bucket < 95:
        return 'offline'
    return 'fault'


def run(database, create_backup=False):
    database = database.resolve(strict=True)
    if create_backup:
        stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
        shutil.copy2(database, database.with_name(database.name + f'.before-demo-status-{stamp}.bak'))

    connection = sqlite3.connect(database.as_uri() + '?mode=rw', uri=True, timeout=30)
    try:
        rows = connection.execute(
            'SELECT source_id,pile_id FROM urbanev_piles WHERE pile_id IS NOT NULL ORDER BY source_id'
        ).fetchall()
        if not rows:
            raise RuntimeError('数据库中没有可更新的 UrbanEV 电桩')
        counts = {'idle': 0, 'charging': 0, 'offline': 0, 'fault': 0}
        connection.execute('BEGIN IMMEDIATE')
        for source_id, pile_id in rows:
            status = demo_status(str(source_id))
            counts[status] += 1
            heartbeat = None if status == 'offline' else datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
            connection.execute(
                "UPDATE charging_piles SET status=?,last_heartbeat_at=?,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?",
                (status, heartbeat, pile_id),
            )
        if connection.execute('PRAGMA foreign_key_check').fetchall():
            raise RuntimeError('外键检查失败')
        if connection.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
            raise RuntimeError('数据库完整性检查失败')
        connection.commit()
        print('updated=' + str(len(rows)))
        for status in ('idle', 'charging', 'offline', 'fault'):
            print(f'{status}={counts[status]}')
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--db', required=True, type=Path)
    parser.add_argument('--backup', action='store_true')
    args = parser.parse_args()
    run(args.db, args.backup)
