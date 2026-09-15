#!/usr/bin/env python3
import os
import stat
from pathlib import Path

root = Path(os.environ.get("CHARGING_ADS_EXPORT", "bigdata/data/exports/ads"))
if not root.exists():
    print("not_run: analytics output directory does not exist")
    raise SystemExit(2)
unsafe = [str(path) for path in root.rglob("*") if path.is_file() and path.stat().st_mode & stat.S_IWOTH]
if unsafe:
    print("world-writable outputs: " + ", ".join(unsafe))
    raise SystemExit(1)
print("analytics output permissions are not world-writable")
