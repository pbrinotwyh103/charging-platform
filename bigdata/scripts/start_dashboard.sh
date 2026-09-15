#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-/opt/module/python/bin/python3.10}"
PID_FILE="$ROOT_DIR/logs/dashboard.pid"
LOG_FILE="$ROOT_DIR/logs/dashboard.log"
mkdir -p "$ROOT_DIR/logs"

if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
  echo "大屏服务已经运行，PID=$(cat "$PID_FILE")"
  exit 0
fi

cd "$ROOT_DIR/dashboard"
nohup "$PYTHON_BIN" app.py >"$LOG_FILE" 2>&1 &
echo $! > "$PID_FILE"
sleep 2

if curl -fsS http://127.0.0.1:5000/api/health >/dev/null; then
  echo "大屏服务启动成功: http://192.168.142.100:5000"
else
  echo "大屏服务启动失败，请查看 $LOG_FILE" >&2
  exit 1
fi

