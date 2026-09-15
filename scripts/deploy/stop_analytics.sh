#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
pid_file="${CHARGING_RUNTIME_DIR:-$repo_root/bigdata/logs}/dashboard.pid"
if [[ ! -f "$pid_file" ]]; then
  echo "analytics dashboard is not running"
  exit 0
fi
pid="$(cat "$pid_file")"
if kill -0 "$pid" 2>/dev/null; then kill "$pid"; fi
rm "$pid_file"
echo "analytics dashboard stopped"
