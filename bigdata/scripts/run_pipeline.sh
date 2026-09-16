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

publish_stage() {
  local layer="$1"
  local row_count="$2"
  shift 2
  local output_args=()
  local output
  for output in "$@"; do
    output_args+=(--output "$output")
  done
  "$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/run_manifest.py" publish \
    --root "$MANIFEST_ROOT" --batch-id "$BATCH_ID" --layer "$layer" \
    --row-count "$row_count" "${output_args[@]}"
}

echo "[$(date '+%F %T')] 生成基于第一阶段和 UrbanEV 的模拟脏数据"
"$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/generate_mock_data.py" --seed "$SEED" --batch-id "$BATCH_ID" 2>&1 | tee "$LOG_DIR/generate_mock_data.log"
RAW_ROWS="$("$PYSPARK_DRIVER_PYTHON" -c 'import json,sys; print(sum(json.load(open(sys.argv[1], encoding="utf-8"))["tables"].values()))' "$ROOT_DIR/data/ods_raw/manifest.json")"
publish_stage raw "$RAW_ROWS" "$ROOT_DIR/data/ods_raw/manifest.json"
run_step "PySpark 数据质量检测" "quality_check.py"
QUALITY_ROWS="$("$PYSPARK_DRIVER_PYTHON" -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["total_rows_scanned"])' "$ROOT_DIR/data/quality/quality_report.json")"
publish_stage quality "$QUALITY_ROWS" "$ROOT_DIR/data/quality/quality_report.json"
run_step "PySpark 数据清洗" "clean_data.py"
CLEAN_ROWS="$("$PYSPARK_DRIVER_PYTHON" -c 'import json,sys; d=json.load(open(sys.argv[1], encoding="utf-8")); print(sum(v["clean"] for v in d.values() if isinstance(v, dict) and "clean" in v))' "$ROOT_DIR/data/clean/cleaning_summary.json")"
publish_stage clean "$CLEAN_ROWS" "$ROOT_DIR/data/clean/cleaning_summary.json"
run_step "SparkSQL ODS-DWD-DWS-ADS 数仓构建" "warehouse_etl.py"
WAREHOUSE_OUTPUTS=(
  "$ROOT_DIR/data/exports/ads/overview.json"
  "$ROOT_DIR/data/exports/ads/revenue_trend.json"
  "$ROOT_DIR/data/exports/ads/station_rank.json"
  "$ROOT_DIR/data/exports/ads/hourly_load.json"
  "$ROOT_DIR/data/exports/ads/pile_status.json"
  "$ROOT_DIR/data/exports/ads/alarm_distribution.json"
  "$ROOT_DIR/data/exports/ads/district_metrics.json"
  "$ROOT_DIR/data/exports/ads/quality.json"
)
ADS_ROWS="$("$PYSPARK_DRIVER_PYTHON" -c 'import json,sys; print(sum(len(v) if isinstance(v, list) else 1 for p in sys.argv[1:] for v in [json.load(open(p, encoding="utf-8"))]))' "${WAREHOUSE_OUTPUTS[@]}")"
publish_stage warehouse "$ADS_ROWS" "${WAREHOUSE_OUTPUTS[@]}"
run_step "Spark MLlib 负荷预测" "train_load_model.py"
PREDICTION_ROWS="$("$PYSPARK_DRIVER_PYTHON" -c 'import json,sys; print(len(json.load(open(sys.argv[1], encoding="utf-8"))))' "$ROOT_DIR/data/exports/ads/predictions.json")"
MODEL_OUTPUTS=(
  "$ROOT_DIR/data/exports/ads/predictions.json"
  "$ROOT_DIR/data/exports/ads/model_metrics.json"
  "$ROOT_DIR/data/exports/ads/model_comparison.json"
  "$ROOT_DIR/data/exports/ads/drift_report.json"
  "$ROOT_DIR/data/exports/ads/scheduling_advice.json"
  "$ROOT_DIR/data/exports/ads/maintenance_advice.json"
  "$ROOT_DIR/data/exports/ads/expansion_advice.json"
  "$ROOT_DIR/data/exports/ads/regulator_summary.json"
)
publish_stage model "$PREDICTION_ROWS" "${MODEL_OUTPUTS[@]}"

echo "[$(date '+%F %T')] 第二阶段数据流水线全部完成"
echo "ADS 输出目录: $ROOT_DIR/data/exports/ads"
trap - ERR
"$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/run_manifest.py" complete --root "$MANIFEST_ROOT" --batch-id "$BATCH_ID"
