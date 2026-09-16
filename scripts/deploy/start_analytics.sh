#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
pid_dir="${CHARGING_RUNTIME_DIR:-$repo_root/bigdata/logs}"
mkdir -p "$pid_dir"
if [[ -f "$pid_dir/dashboard.pid" ]] && kill -0 "$(cat "$pid_dir/dashboard.pid")" 2>/dev/null; then
  echo "analytics dashboard already running"
  exit 0
fi
cd "$repo_root"
nohup "${CHARGING_PYTHON:-python3}" -m bigdata.dashboard.app >"$pid_dir/dashboard.log" 2>&1 &
echo "$!" >"$pid_dir/dashboard.pid"
echo "analytics dashboard started, pid=$!"
