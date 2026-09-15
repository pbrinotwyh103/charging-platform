#!/bin/bash
set -eo pipefail

PROFILE="course-vm"
SEED="20260915"
BATCH_ID="$(date -u '+%Y%m%dT%H%M%SZ')"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --local) PROFILE="local"; shift ;;
    --seed) SEED="$2"; shift 2 ;;
    --batch-id) BATCH_ID="$2"; shift 2 ;;
    *) echo "未知参数: $1" >&2; exit 2 ;;
  esac
done

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PIPELINE_DIR="$ROOT_DIR/pipeline"
LOG_DIR="$ROOT_DIR/logs"
mkdir -p "$LOG_DIR"

if [[ "$PROFILE" == "course-vm" ]]; then
  source /home/hadoop/.bash_profile
fi
export PYSPARK_PYTHON="${PYSPARK_PYTHON:-/opt/module/python/bin/python3.10}"
if [[ "$PROFILE" == "local" && ! -x "$PYSPARK_PYTHON" ]]; then
  PYSPARK_PYTHON="$(command -v python3)"
fi
export PYSPARK_DRIVER_PYTHON="$PYSPARK_PYTHON"
export SPARK_MASTER="${SPARK_MASTER:-local[2]}"
export CHARGING_SOURCE_DB="${CHARGING_SOURCE_DB:-$ROOT_DIR/../database/charging.db}"
export CHARGING_BATCH_ID="$BATCH_ID"
export CHARGING_DATA_VERSION="${CHARGING_DATA_VERSION:-$BATCH_ID}"
MANIFEST_ROOT="$ROOT_DIR/data/run_manifests"
INPUT_ARGS=()
[[ -f "$CHARGING_SOURCE_DB" ]] && INPUT_ARGS+=(--input "$CHARGING_SOURCE_DB")
"$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/run_manifest.py" start --root "$MANIFEST_ROOT" --batch-id "$BATCH_ID" "${INPUT_ARGS[@]}"
CURRENT_STAGE="generate"
on_error() {
  "$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/run_manifest.py" fail --root "$MANIFEST_ROOT" --batch-id "$BATCH_ID" --stage "$CURRENT_STAGE" --message "pipeline command failed"
}
trap on_error ERR

if [[ "$PROFILE" == "course-vm" ]] && ! jps | grep -q NameNode; then
  /opt/module/hadoop-3.3.0/sbin/start-all.sh
  sleep 8
fi
if [[ "$PROFILE" == "course-vm" ]] && ! jps -l | grep -q 'org.apache.spark.deploy.master.Master'; then
  /opt/module/spark-3.4.1/sbin/start-all.sh
  sleep 5
fi
[[ "$PROFILE" == "course-vm" ]] && hdfs dfsadmin -safemode leave >/dev/null 2>&1 || true

run_step() {
  local name="$1"
  local script="$2"
  CURRENT_STAGE="${script%.py}"
  echo "[$(date '+%F %T')] $name"
  spark-submit --master "$SPARK_MASTER" "$PIPELINE_DIR/$script" 2>&1 | tee "$LOG_DIR/${script%.py}.log"
}

echo "[$(date '+%F %T')] 生成基于第一阶段和 UrbanEV 的模拟脏数据"
"$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/generate_mock_data.py" --seed "$SEED" --batch-id "$BATCH_ID" 2>&1 | tee "$LOG_DIR/generate_mock_data.log"
run_step "PySpark 数据质量检测" "quality_check.py"
run_step "PySpark 数据清洗" "clean_data.py"
run_step "SparkSQL ODS-DWD-DWS-ADS 数仓构建" "warehouse_etl.py"
run_step "Spark MLlib 负荷预测" "train_load_model.py"

echo "[$(date '+%F %T')] 第二阶段数据流水线全部完成"
echo "ADS 输出目录: $ROOT_DIR/data/exports/ads"
trap - ERR
"$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/run_manifest.py" complete --root "$MANIFEST_ROOT" --batch-id "$BATCH_ID"
