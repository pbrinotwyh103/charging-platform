#!/bin/bash
set -eo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PIPELINE_DIR="$ROOT_DIR/pipeline"
LOG_DIR="$ROOT_DIR/logs"
mkdir -p "$LOG_DIR"

source /home/hadoop/.bash_profile
export PYSPARK_PYTHON="${PYSPARK_PYTHON:-/opt/module/python/bin/python3.10}"
export PYSPARK_DRIVER_PYTHON="$PYSPARK_PYTHON"
export SPARK_MASTER="${SPARK_MASTER:-local[2]}"
export CHARGING_SOURCE_DB="${CHARGING_SOURCE_DB:-$ROOT_DIR/../database/charging.db}"

if ! jps | grep -q NameNode; then
  /opt/module/hadoop-3.3.0/sbin/start-all.sh
  sleep 8
fi
if ! jps -l | grep -q 'org.apache.spark.deploy.master.Master'; then
  /opt/module/spark-3.4.1/sbin/start-all.sh
  sleep 5
fi
hdfs dfsadmin -safemode leave >/dev/null 2>&1 || true

run_step() {
  local name="$1"
  local script="$2"
  echo "[$(date '+%F %T')] $name"
  spark-submit --master "$SPARK_MASTER" "$PIPELINE_DIR/$script" 2>&1 | tee "$LOG_DIR/${script%.py}.log"
}

echo "[$(date '+%F %T')] 生成基于第一阶段和 UrbanEV 的模拟脏数据"
"$PYSPARK_DRIVER_PYTHON" "$PIPELINE_DIR/generate_mock_data.py" 2>&1 | tee "$LOG_DIR/generate_mock_data.log"
run_step "PySpark 数据质量检测" "quality_check.py"
run_step "PySpark 数据清洗" "clean_data.py"
run_step "SparkSQL ODS-DWD-DWS-ADS 数仓构建" "warehouse_etl.py"
run_step "Spark MLlib 负荷预测" "train_load_model.py"

echo "[$(date '+%F %T')] 第二阶段数据流水线全部完成"
echo "ADS 输出目录: $ROOT_DIR/data/exports/ads"
