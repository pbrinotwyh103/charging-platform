"""使用 PySpark 清洗原始数据，并将不可修复记录写入隔离区。"""

from __future__ import annotations

from functools import reduce

from pyspark.sql import DataFrame, Window
from pyspark.sql import functions as F

from common import CLEAN_ROOT, RAW_ROOT, create_spark, ensure_local_directories, local_uri, write_json, with_run_metadata


def load_raw(spark, table: str) -> DataFrame:
    return spark.read.option("header", True).option("inferSchema", False).csv(
        local_uri(RAW_ROOT / f"{table}.csv")
    )


def keep_first(df: DataFrame, keys: list[str]) -> DataFrame:
    window = Window.partitionBy(*keys).orderBy(F.col("_row_id").asc())
    return df.withColumn("_rn", F.row_number().over(window)).filter("_rn = 1").drop("_rn")


def quarantine(df: DataFrame, table: str, reason: str, condition) -> DataFrame:
    return (
        df.filter(condition)
        .select(
            F.lit(table).alias("table_name"),
            F.col("_row_id").alias("row_id"),
            F.lit(reason).alias("reject_reason"),
            F.to_json(F.struct(*[F.col(name) for name in df.columns])).alias("raw_record"),
        )
    )


def write_clean(df: DataFrame, table: str) -> None:
    target = CLEAN_ROOT / table
    df = with_run_metadata(df)
    df.write.mode("overwrite").parquet(local_uri(target / "parquet"))
    df.coalesce(1).write.mode("overwrite").option("header", True).csv(local_uri(target / "csv"))


def main() -> None:
    ensure_local_directories()
    spark = create_spark("charging-data-cleaning")
    spark.sparkContext.setLogLevel("WARN")
    rejected: list[DataFrame] = []
    stats: dict[str, dict] = {}

    raw_users = load_raw(spark, "users")
    users = (
        keep_first(raw_users, ["id"])
        .transform(lambda df: keep_first(df, ["phone"]))
        .withColumn("id", F.col("id").cast("long"))
        .withColumn("balance_cents", F.col("balance_cents").cast("long"))
        .withColumn("created_at", F.to_timestamp("created_at"))
    )
    invalid_users = (
        users.id.isNull()
        | users.phone.isNull()
        | (~users.phone.rlike(r"^1\d{10}$"))
        | users.created_at.isNull()
    )
    rejected.append(quarantine(users, "users", "用户主键、手机号或注册时间不可修复", invalid_users))
    clean_users = (
        users.filter(~invalid_users)
        .withColumn(
            "nickname",
            F.when(F.trim("nickname") == "", F.concat(F.lit("用户"), F.col("id"))).otherwise(F.col("nickname")),
        )
        .withColumn("balance_cents", F.greatest(F.coalesce("balance_cents", F.lit(0)), F.lit(0)))
        .withColumn("status", F.when(F.col("status").isin("normal", "frozen"), F.col("status")).otherwise(F.lit("normal")))
        .drop("_row_id")
    )
    write_clean(clean_users, "users")
    stats["users"] = {"raw": raw_users.count(), "clean": clean_users.count()}

    raw_stations = load_raw(spark, "stations")
    stations = (
        keep_first(raw_stations, ["id"])
        .withColumn("id", F.col("id").cast("long"))
        .withColumn("longitude", F.col("longitude").cast("double"))
        .withColumn("latitude", F.col("latitude").cast("double"))
        .withColumn("price_cents_per_kwh", F.col("price_cents_per_kwh").cast("int"))
        .withColumn("created_at", F.to_timestamp("created_at"))
    )
    invalid_stations = (
        stations.id.isNull()
        | stations.name.isNull()
        | (F.trim(stations.name) == "")
        | (~stations.longitude.between(113.6, 114.7))
        | (~stations.latitude.between(22.3, 23.0))
        | (stations.price_cents_per_kwh <= 0)
    )
    rejected.append(quarantine(stations, "stations", "站点名称、位置或价格不合法", invalid_stations))
    clean_stations = (
        stations.filter(~invalid_stations)
        .withColumn("district", F.coalesce(F.col("district"), F.lit("未知区域")))
        .withColumn("status", F.when(F.col("status").isin("online", "offline"), F.col("status")).otherwise(F.lit("offline")))
        .drop("_row_id")
    )
    write_clean(clean_stations, "stations")
    stats["stations"] = {"raw": raw_stations.count(), "clean": clean_stations.count()}

    raw_piles = load_raw(spark, "charging_piles")
    piles = (
        keep_first(raw_piles, ["pile_code"])
        .withColumn("id", F.col("id").cast("long"))
        .withColumn("station_id", F.col("station_id").cast("long"))
        .withColumn("power_kw", F.col("power_kw").cast("double"))
        .withColumn("total_charge_count", F.col("total_charge_count").cast("long"))
        .withColumn("total_charge_seconds", F.col("total_charge_seconds").cast("long"))
        .withColumn("last_heartbeat_at", F.to_timestamp("last_heartbeat_at"))
    )
    valid_station_ids = clean_stations.select(F.col("id").alias("valid_station_id"))
    piles = piles.join(valid_station_ids, piles.station_id == F.col("valid_station_id"), "left")
    invalid_piles = (
        piles.id.isNull()
        | piles.pile_code.isNull()
        | (F.trim(piles.pile_code) == "")
        | piles.valid_station_id.isNull()
        | (piles.power_kw <= 0)
        | (~piles.charge_type.isin("fast", "slow"))
    )
    rejected.append(quarantine(piles, "charging_piles", "充电桩主键、所属站点、类型或功率不合法", invalid_piles))
    clean_piles = (
        piles.filter(~invalid_piles)
        .withColumn(
            "status",
            F.when(
                F.col("status").isin("idle", "reserved", "charging", "fault", "offline", "disabled"),
                F.col("status"),
            ).otherwise(F.lit("offline")),
        )
        .withColumn("total_charge_count", F.greatest(F.coalesce("total_charge_count", F.lit(0)), F.lit(0)))
        .withColumn("total_charge_seconds", F.greatest(F.coalesce("total_charge_seconds", F.lit(0)), F.lit(0)))
        .drop("_row_id", "valid_station_id")
    )
    write_clean(clean_piles, "charging_piles")
    stats["charging_piles"] = {"raw": raw_piles.count(), "clean": clean_piles.count()}

    raw_orders = load_raw(spark, "charging_orders")
    orders = (
        keep_first(raw_orders, ["order_no"])
        .withColumn("id", F.col("id").cast("long"))
        .withColumn("user_id", F.col("user_id").cast("long"))
        .withColumn("station_id", F.col("station_id").cast("long"))
        .withColumn("pile_id", F.col("pile_id").cast("long"))
        .withColumn("started_at", F.to_timestamp("started_at"))
        .withColumn("stopped_at", F.to_timestamp("stopped_at"))
        .withColumn("duration_seconds", F.col("duration_seconds").cast("long"))
        .withColumn("energy_wh", F.col("energy_wh").cast("long"))
        .withColumn("unit_price_cents", F.col("unit_price_cents").cast("int"))
        .withColumn("fee_cents", F.col("fee_cents").cast("long"))
    )
    valid_users = clean_users.select(F.col("id").alias("valid_user_id"))
    valid_order_stations = clean_stations.select(F.col("id").alias("valid_station_id"))
    valid_piles = clean_piles.select(
        F.col("id").alias("valid_pile_id"),
        F.col("station_id").alias("pile_station_id"),
    )
    orders = (
        orders.join(valid_users, orders.user_id == F.col("valid_user_id"), "left")
        .join(valid_order_stations, orders.station_id == F.col("valid_station_id"), "left")
        .join(valid_piles, orders.pile_id == F.col("valid_pile_id"), "left")
    )
    invalid_orders = (
        orders.id.isNull()
        | orders.order_no.isNull()
        | orders.valid_user_id.isNull()
        | orders.valid_station_id.isNull()
        | orders.valid_pile_id.isNull()
        | (orders.station_id != orders.pile_station_id)
        | orders.started_at.isNull()
        | orders.stopped_at.isNull()
        | (orders.stopped_at < orders.started_at)
        | (orders.duration_seconds < 0)
        | (orders.energy_wh < 0)
        | (orders.energy_wh > 500000)
        | (orders.fee_cents < 0)
        | (~orders.status.isin("charging", "completed", "fault_stopped", "cancelled"))
    )
    rejected.append(quarantine(orders, "charging_orders", "订单主外键、时间、状态或计量数据不可修复", invalid_orders))
    clean_orders = (
        orders.filter(~invalid_orders)
        .withColumn("duration_seconds", F.when(F.col("status") == "cancelled", F.lit(0)).otherwise(F.greatest(F.col("duration_seconds"), F.unix_timestamp("stopped_at") - F.unix_timestamp("started_at"))))
        .withColumn("energy_wh", F.when(F.col("status") == "cancelled", F.lit(0)).otherwise(F.col("energy_wh")))
        .withColumn("fee_cents", F.when(F.col("status") == "cancelled", F.lit(0)).otherwise(F.round(F.col("energy_wh") / 1000 * F.col("unit_price_cents")).cast("long")))
        .withColumn("order_date", F.to_date("started_at"))
        .withColumn("start_hour", F.hour("started_at"))
        .drop("_row_id", "valid_user_id", "valid_station_id", "valid_pile_id", "pile_station_id")
    )
    write_clean(clean_orders, "charging_orders")
    stats["charging_orders"] = {"raw": raw_orders.count(), "clean": clean_orders.count()}

    raw_alarms = load_raw(spark, "alarms")
    alarms = (
        keep_first(raw_alarms, ["id"])
        .withColumn("id", F.col("id").cast("long"))
        .withColumn("pile_id", F.col("pile_id").cast("long"))
        .withColumn("order_id", F.col("order_id").cast("long"))
        .withColumn("occurred_at", F.to_timestamp("occurred_at"))
        .withColumn("recovered_at", F.to_timestamp("recovered_at"))
        .join(clean_piles.select(F.col("id").alias("valid_pile_id")), F.col("pile_id") == F.col("valid_pile_id"), "left")
    )
    invalid_alarms = alarms.id.isNull() | alarms.valid_pile_id.isNull() | alarms.occurred_at.isNull() | alarms.alarm_type.isNull() | (F.trim(alarms.alarm_type) == "")
    rejected.append(quarantine(alarms, "alarms", "告警主键、充电桩或时间不可修复", invalid_alarms))
    clean_alarms = (
        alarms.filter(~invalid_alarms)
        .withColumn("severity", F.when(F.col("severity").isin("info", "warning", "critical"), F.col("severity")).otherwise(F.lit("warning")))
        .withColumn("status", F.when(F.col("status").isin("open", "acknowledged", "resolved"), F.col("status")).otherwise(F.lit("open")))
        .drop("_row_id", "valid_pile_id")
    )
    write_clean(clean_alarms, "alarms")
    stats["alarms"] = {"raw": raw_alarms.count(), "clean": clean_alarms.count()}

    raw_weather = load_raw(spark, "weather_daily")
    weather = (
        keep_first(raw_weather, ["date"])
        .withColumn("date", F.to_date("date"))
        .withColumn("temperature_c", F.col("temperature_c").cast("double"))
        .withColumn("rainfall_mm", F.col("rainfall_mm").cast("double"))
        .withColumn("is_holiday", F.col("is_holiday").cast("int"))
    )
    median_temp = weather.filter(F.col("temperature_c").between(-10, 50)).approxQuantile("temperature_c", [0.5], 0.01)[0]
    clean_weather = (
        weather.filter(F.col("date").isNotNull())
        .withColumn("temperature_c", F.when(F.col("temperature_c").between(-10, 50), F.col("temperature_c")).otherwise(F.lit(median_temp)))
        .withColumn("rainfall_mm", F.when(F.col("rainfall_mm") >= 0, F.col("rainfall_mm")).otherwise(F.lit(0.0)))
        .withColumn("is_holiday", F.when(F.col("is_holiday").isin(0, 1), F.col("is_holiday")).otherwise(F.lit(0)))
        .drop("_row_id")
    )
    write_clean(clean_weather, "weather_daily")
    stats["weather_daily"] = {"raw": raw_weather.count(), "clean": clean_weather.count()}

    all_rejected = reduce(lambda left, right: left.unionByName(right), rejected).cache()
    all_rejected.write.mode("overwrite").parquet(local_uri(CLEAN_ROOT / "quarantine" / "parquet"))
    all_rejected.coalesce(1).write.mode("overwrite").option("header", True).csv(local_uri(CLEAN_ROOT / "quarantine" / "csv"))

    for table, values in stats.items():
        values["rejected_or_deduplicated"] = values["raw"] - values["clean"]
        values["retention_rate_percent"] = round(values["clean"] * 100 / max(values["raw"], 1), 4)
    stats["quarantine_total"] = {"rows": all_rejected.count()}
    write_json(CLEAN_ROOT / "cleaning_summary.json", stats)
    print("PySpark 数据清洗完成")
    for table, values in stats.items():
        print(f"{table}: {values}")
    spark.stop()


if __name__ == "__main__":
    main()
