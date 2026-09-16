#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-/opt/module/python/bin/python3.10}"
VENDOR_DIR="$ROOT_DIR/dashboard/static/vendor"
mkdir -p "$VENDOR_DIR"

"$PYTHON_BIN" -m pip install --user -r "$ROOT_DIR/requirements.txt"

download_if_missing() {
  local url="$1"
  local target="$2"
  if [ ! -s "$target" ]; then
    curl -fL --retry 3 --connect-timeout 10 "$url" -o "$target"
  fi
}

download_if_missing "https://cdn.jsdelivr.net/npm/vue@3.5.13/dist/vue.global.prod.js" "$VENDOR_DIR/vue.global.prod.js"
download_if_missing "https://cdn.jsdelivr.net/npm/echarts@5.6.0/dist/echarts.min.js" "$VENDOR_DIR/echarts.min.js"

echo "Flask、Vue 和 ECharts 环境准备完成"

