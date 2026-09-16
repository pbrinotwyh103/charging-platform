"""使用 SparkSQL 构建 ODS、DWD、DWS、ADS 四层数据仓库。"""

from __future__ import annotations

from datetime import date, datetime
from decimal import Decimal

from pyspark.sql import DataFrame
from pyspark.sql import functions as F

from common import (
    CLEAN_ROOT,
    EXPORT_ROOT,
    QUALITY_ROOT,
    RAW_ROOT,
    WAREHOUSE_ROOT,
    create_spark,
    ensure_local_directories,
    hdfs_path,
    local_uri,
    read_json,
    write_json,
    with_run_metadata,
)


TABLES = (
    "users",
    "stations",
    "charging_piles",
    "charging_orders",
    "alarms",
    "weather_daily",
)


def write_layer(df: DataFrame, layer: str, table: str) -> None:
    local_target = WAREHOUSE_ROOT / layer / table
    df = with_run_metadata(df)
    df.write.mode("overwrite").parquet(local_uri(local_target))
    df.write.mode("overwrite").parquet(hdfs_path(layer, table))


def serializable(value):
    if isinstance(value, (date, datetime)):
        return value.isoformat()
    if isinstance(value, Decimal):
        return float(value)
    return value


def rows_to_json(df: DataFrame, filename: str, limit: int = 5000) -> list[dict]:
    rows = []
    for row in df.limit(limit).collect():
        rows.append({key: serializable(value) for key, value in row.asDict().items()})
    write_json(EXPORT_ROOT / filename, rows)
    return rows


def main() -> None:
    ensure_local_directories()
    spark = create_spark("charging-spark-sql-warehouse")
    spark.sparkContext.setLogLevel("WARN")

    ods: dict[str, DataFrame] = {}
    dwd: dict[str, DataFrame] = {}
    for table in TABLES:
        ods_df = spark.read.option("header", True).option("inferSchema", False).csv(
            local_uri(RAW_ROOT / f"{table}.csv")
        ).withColumn("ods_ingest_time", F.current_timestamp())
        ods[table] = ods_df
        write_layer(ods_df, "ods", f"ods_{table}")

        dwd_df = spark.read.parquet(local_uri(CLEAN_ROOT / table / "parquet"))
        dwd[table] = dwd_df
        write_layer(dwd_df, "dwd", f"dwd_{table}")
        dwd_df.createOrReplaceTempView(f"dwd_{table}")

    # DWS 站点日粒度经营指标。
    dws_station_day = spark.sql(
        """
        SELECT
            o.order_date,
            o.station_id,
            s.name AS station_name,
            s.district,
            s.longitude,
            s.latitude,
            s.price_cents_per_kwh,
            COUNT(*) AS session_count,
            SUM(CASE WHEN o.status = 'completed' THEN 1 ELSE 0 END) AS completed_count,
            SUM(CASE WHEN o.status = 'fault_stopped' THEN 1 ELSE 0 END) AS fault_stopped_count,
            COUNT(DISTINCT o.user_id) AS active_user_count,
            ROUND(SUM(o.energy_wh) / 1000.0, 3) AS energy_kwh,
            ROUND(SUM(CASE WHEN o.status = 'completed' THEN o.fee_cents ELSE 0 END) / 100.0, 2) AS revenue_yuan,
            ROUND(AVG(CASE WHEN o.duration_seconds > 0 THEN o.duration_seconds END) / 60.0, 2) AS avg_duration_minutes,
            ROUND(SUM(CASE WHEN o.status = 'completed' THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS completion_rate_percent
        FROM dwd_charging_orders o
        JOIN dwd_stations s ON o.station_id = s.id
        GROUP BY o.order_date, o.station_id, s.name, s.district,
                 s.longitude, s.latitude, s.price_cents_per_kwh
        """
    )
    write_layer(dws_station_day, "dws", "dws_station_day")
    dws_station_day.createOrReplaceTempView("dws_station_day")

    dws_city_day = spark.sql(
        """
        SELECT
            order_date,
            SUM(session_count) AS session_count,
            SUM(completed_count) AS completed_count,
            SUM(fault_stopped_count) AS fault_stopped_count,
            SUM(active_user_count) AS station_user_visits,
            ROUND(SUM(energy_kwh), 3) AS energy_kwh,
            ROUND(SUM(revenue_yuan), 2) AS revenue_yuan,
            ROUND(AVG(avg_duration_minutes), 2) AS avg_duration_minutes,
            ROUND(SUM(completed_count) * 100.0 / SUM(session_count), 2) AS completion_rate_percent
        FROM dws_station_day
        GROUP BY order_date
        ORDER BY order_date
        """
    )
    write_layer(dws_city_day, "dws", "dws_city_day")
    dws_city_day.createOrReplaceTempView("dws_city_day")

    pile_capacity = spark.sql(
        """
        SELECT station_id, COUNT(*) AS pile_count,
               SUM(CASE WHEN charge_type='fast' THEN 1 ELSE 0 END) AS fast_pile_count,
               SUM(CASE WHEN status IN ('fault','offline','disabled') THEN 1 ELSE 0 END) AS unavailable_pile_count
        FROM dwd_charging_piles
        GROUP BY station_id
        """
    )
    pile_capacity.createOrReplaceTempView("pile_capacity")

    dws_station_hour = spark.sql(
        """
        SELECT
            o.order_date,
            o.start_hour,
            o.station_id,
            s.name AS station_name,
            s.district,
            s.price_cents_per_kwh,
            p.pile_count,
            p.fast_pile_count,
            p.unavailable_pile_count,
            COUNT(*) AS session_count,
            COUNT(DISTINCT o.user_id) AS active_user_count,
            ROUND(SUM(o.energy_wh) / 1000.0, 3) AS energy_kwh,
            ROUND(SUM(CASE WHEN o.status='completed' THEN o.fee_cents ELSE 0 END) / 100.0, 2) AS revenue_yuan,
            w.temperature_c,
            w.rainfall_mm,
            w.is_holiday,
            DAYOFWEEK(o.order_date) AS day_of_week,
            CASE WHEN DAYOFWEEK(o.order_date) IN (1, 7) THEN 1 ELSE 0 END AS is_weekend
        FROM dwd_charging_orders o
        JOIN dwd_stations s ON o.station_id = s.id
        JOIN pile_capacity p ON o.station_id = p.station_id
        LEFT JOIN dwd_weather_daily w ON o.order_date = w.date
        WHERE o.status <> 'cancelled'
        GROUP BY o.order_date, o.start_hour, o.station_id, s.name, s.district,
                 s.price_cents_per_kwh, p.pile_count, p.fast_pile_count,
                 p.unavailable_pile_count, w.temperature_c, w.rainfall_mm,
                 w.is_holiday, DAYOFWEEK(o.order_date)
        """
    )
    write_layer(dws_station_hour, "dws", "dws_station_hour")
    dws_station_hour.createOrReplaceTempView("dws_station_hour")

    dws_user_value = spark.sql(
        """
        SELECT
            u.id AS user_id,
            u.phone,
            u.nickname,
            COUNT(o.id) AS order_count,
            ROUND(COALESCE(SUM(o.energy_wh), 0) / 1000.0, 3) AS total_energy_kwh,
            ROUND(COALESCE(SUM(CASE WHEN o.status='completed' THEN o.fee_cents ELSE 0 END), 0) / 100.0, 2) AS total_spend_yuan,
            MAX(o.started_at) AS last_charge_at,
            CASE
                WHEN COUNT(o.id) >= 100 THEN 'high_value'
                WHEN COUNT(o.id) >= 30 THEN 'active'
                ELSE 'normal'
            END AS user_segment
        FROM dwd_users u
        LEFT JOIN dwd_charging_orders o ON u.id = o.user_id
        GROUP BY u.id, u.phone, u.nickname
        """
    )
    write_layer(dws_user_value, "dws", "dws_user_value")

    # ADS：面向 Flask 大屏的结果集。
    ads_overview = spark.sql(
        """
        SELECT
            (SELECT COUNT(*) FROM dwd_stations) AS station_count,
            (SELECT COUNT(*) FROM dwd_charging_piles) AS pile_count,
            (SELECT COUNT(*) FROM dwd_users) AS user_count,
            COUNT(*) AS order_count,
            SUM(CASE WHEN status='completed' THEN 1 ELSE 0 END) AS completed_order_count,
            ROUND(SUM(energy_wh) / 1000.0, 2) AS total_energy_kwh,
            ROUND(SUM(CASE WHEN status='completed' THEN fee_cents ELSE 0 END) / 100.0, 2) AS total_revenue_yuan,
            ROUND(SUM(CASE WHEN status='completed' THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS completion_rate_percent,
            ROUND(AVG(CASE WHEN status='completed' THEN fee_cents END) / 100.0, 2) AS avg_order_fee_yuan
        FROM dwd_charging_orders
        """
    )
    write_layer(ads_overview, "ads", "ads_overview")

    ads_revenue_trend = spark.sql(
        """
        SELECT order_date AS date, session_count AS orders,
               energy_kwh, revenue_yuan, completion_rate_percent
        FROM dws_city_day
        WHERE order_date >= DATE_SUB((SELECT MAX(order_date) FROM dws_city_day), 29)
        ORDER BY order_date
        """
    )
    write_layer(ads_revenue_trend, "ads", "ads_revenue_trend")

    ads_station_rank = spark.sql(
        """
        SELECT station_id, station_name, district,
               SUM(session_count) AS orders,
               ROUND(SUM(energy_kwh), 2) AS energy_kwh,
               ROUND(SUM(revenue_yuan), 2) AS revenue_yuan,
               ROUND(AVG(completion_rate_percent), 2) AS completion_rate_percent
        FROM dws_station_day
        GROUP BY station_id, station_name, district
        ORDER BY revenue_yuan DESC
        LIMIT 10
        """
    )
    write_layer(ads_station_rank, "ads", "ads_station_rank")

    ads_hourly_load = spark.sql(
        """
        SELECT start_hour AS hour,
               ROUND(AVG(session_count), 2) AS avg_sessions,
               ROUND(AVG(energy_kwh), 2) AS avg_energy_kwh,
               ROUND(AVG(revenue_yuan), 2) AS avg_revenue_yuan
        FROM dws_station_hour
        GROUP BY start_hour
        ORDER BY start_hour
        """
    )
    write_layer(ads_hourly_load, "ads", "ads_hourly_load")

    ads_pile_status = spark.sql(
        """
        SELECT status, COUNT(*) AS count,
               ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER (), 2) AS percentage
        FROM dwd_charging_piles
        GROUP BY status
        ORDER BY count DESC
        """
    )
    write_layer(ads_pile_status, "ads", "ads_pile_status")

    ads_alarm_distribution = spark.sql(
        """
        SELECT severity, status, COUNT(*) AS count
        FROM dwd_alarms
        GROUP BY severity, status
        ORDER BY severity, status
        """
    )
    write_layer(ads_alarm_distribution, "ads", "ads_alarm_distribution")

    ads_district = spark.sql(
        """
        SELECT district,
               COUNT(DISTINCT station_id) AS active_station_count,
               SUM(session_count) AS orders,
               ROUND(SUM(energy_kwh), 2) AS energy_kwh,
               ROUND(SUM(revenue_yuan), 2) AS revenue_yuan
        FROM dws_station_day
        GROUP BY district
        ORDER BY revenue_yuan DESC
        """
    )
    write_layer(ads_district, "ads", "ads_district")

    rows_to_json(ads_overview, "overview.json")
    rows_to_json(ads_revenue_trend, "revenue_trend.json")
    rows_to_json(ads_station_rank, "station_rank.json")
    rows_to_json(ads_hourly_load, "hourly_load.json")
    rows_to_json(ads_pile_status, "pile_status.json")
    rows_to_json(ads_alarm_distribution, "alarm_distribution.json")
    rows_to_json(ads_district, "district_metrics.json")

    quality_report = read_json(QUALITY_ROOT / "quality_report.json", {})
    cleaning_summary = read_json(CLEAN_ROOT / "cleaning_summary.json", {})
    write_json(
        EXPORT_ROOT / "quality.json",
        {
            "total_rows_scanned": quality_report.get("total_rows_scanned", 0),
            "total_issues": quality_report.get("total_issues", 0),
            "issue_rate_percent": quality_report.get("issue_rate_percent", 0),
            "rule_summary": quality_report.get("rule_summary", []),
            "cleaning_summary": cleaning_summary,
        },
    )

    print(f"SparkSQL 数仓构建完成，本地目录: {WAREHOUSE_ROOT}")
    print(f"HDFS 根目录: {hdfs_path('')}")
    print(f"ADS 可视化数据: {EXPORT_ROOT}")
    spark.stop()


if __name__ == "__main__":
    main()
