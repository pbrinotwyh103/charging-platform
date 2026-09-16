"""第二阶段大数据处理公共配置。"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from pyspark.sql import SparkSession


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_ROOT = Path(os.environ.get("CHARGING_BIGDATA_DATA", PROJECT_ROOT / "data"))
RAW_ROOT = DATA_ROOT / "ods_raw"
CLEAN_ROOT = DATA_ROOT / "clean"
QUALITY_ROOT = DATA_ROOT / "quality"
WAREHOUSE_ROOT = DATA_ROOT / "warehouse"
EXPORT_ROOT = DATA_ROOT / "exports" / "ads"
MODEL_ROOT = DATA_ROOT / "models" / "station_load_rf"

SOURCE_DB = Path(
    os.environ.get(
        "CHARGING_SOURCE_DB",
        PROJECT_ROOT.parent / "database" / "charging.db",
    )
)
SOURCE_SNAPSHOT_ROOT = PROJECT_ROOT / "source"

HDFS_BASE = os.environ.get(
    "CHARGING_HDFS_BASE", "hdfs://node100:9000/charging_platform"
).rstrip("/")


def ensure_local_directories() -> None:
    for path in (
        RAW_ROOT,
        CLEAN_ROOT,
        QUALITY_ROOT,
        WAREHOUSE_ROOT,
        EXPORT_ROOT,
        MODEL_ROOT.parent,
    ):
        path.mkdir(parents=True, exist_ok=True)


def create_spark(app_name: str) -> "SparkSession":
    """创建适合单机教学虚拟机的 SparkSession。"""
    from pyspark.sql import SparkSession

    master = os.environ.get("SPARK_MASTER", "local[2]")
    return (
        SparkSession.builder.appName(app_name)
        .master(master)
        .config("spark.sql.session.timeZone", "Asia/Shanghai")
        .config("spark.sql.shuffle.partitions", "8")
        .config("spark.default.parallelism", "4")
        .config("spark.driver.memory", os.environ.get("SPARK_DRIVER_MEMORY", "1g"))
        .getOrCreate()
    )


def hdfs_path(layer: str, table: str | None = None) -> str:
    base = f"{HDFS_BASE}/{layer.strip('/')}"
    return f"{base}/{table}" if table else base


def local_uri(path: Path) -> str:
    """显式生成 file:// URI，避免 Hadoop 默认文件系统把本地路径解释为 HDFS。"""
    return path.resolve().as_uri()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, default=str),
        encoding="utf-8",
    )


def read_json(path: Path, default: Any) -> Any:
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8"))


def with_run_metadata(dataframe: Any) -> Any:
    """Append traceability fields without importing PySpark at module import time."""
    from pyspark.sql import functions as F
    try:
        from .run_manifest import current_run_metadata
    except ImportError:
        from run_manifest import current_run_metadata

    metadata = current_run_metadata()
    return (dataframe.withColumn("batch_id", F.lit(metadata["batch_id"]))
            .withColumn("data_version", F.lit(metadata["data_version"]))
            .withColumn("generated_at", F.to_timestamp(F.lit(metadata["generated_at"]))))
