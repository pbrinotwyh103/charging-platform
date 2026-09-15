"""使用 PySpark 发现充电业务原始数据中的质量问题。"""

from __future__ import annotations

from functools import reduce

from pyspark.sql import DataFrame, Window
from pyspark.sql import functions as F

from common import QUALITY_ROOT, RAW_ROOT, create_spark, ensure_local_directories, local_uri, write_json
from run_manifest import current_run_metadata


def load_raw(spark, table: str) -> DataFrame:
    return (
        spark.read.option("header", True)
        .option("encoding", "UTF-8")
        .option("inferSchema", False)
        .csv(local_uri(RAW_ROOT / f"{table}.csv"))
    )


def make_issue(
    df: DataFrame,
    table: str,
    rule_id: str,
    category: str,
    field: str,
    severity: str,
    condition,
    problem: str,
) -> DataFrame:
    value = F.col(field).cast("string") if field in df.columns else F.lit("")
    return (
        df.filter(condition)
        .select(
            F.lit(table).alias("table_name"),
            F.lit(rule_id).alias("rule_id"),
            F.lit(category).alias("category"),
            F.lit(field).alias("field_name"),
            F.lit(severity).alias("severity"),
            F.col("_row_id").alias("row_id"),
            F.lit(problem).alias("problem"),
            value.alias("detected_value"),
        )
    )


def duplicate_issue(df: DataFrame, table: str, field: str, rule_id: str) -> DataFrame:
    window = Window.partitionBy(field)
    marked = df.withColumn("_duplicate_count", F.count("*").over(window))
    return make_issue(
        marked,
        table,
        rule_id,
        "唯一性",
        field,
        "critical",
        (F.col(field).isNotNull())
        & (F.trim(F.col(field)) != "")
        & (F.col("_duplicate_count") > 1),
        f"{field} 业务主键重复",
    )


def main() -> None:
    ensure_local_directories()
    spark = create_spark("charging-quality-discovery")
    spark.sparkContext.setLogLevel("WARN")

    tables = {
        name: load_raw(spark, name)
        for name in (
            "users",
            "stations",
            "charging_piles",
            "charging_orders",
            "alarms",
            "weather_daily",
        )
    }
    users = tables["users"]
    stations = tables["stations"]
    piles = tables["charging_piles"]
    orders = tables["charging_orders"]
    alarms = tables["alarms"]
    weather = tables["weather_daily"]

    issues: list[DataFrame] = []
    add = issues.append

    # 用户表：完整性、格式、范围、枚举、唯一性。
    add(make_issue(users, "users", "USR001", "完整性", "phone", "critical", F.col("phone").isNull() | (F.trim("phone") == ""), "手机号不能为空"))
    add(make_issue(users, "users", "USR002", "有效性", "phone", "critical", ~F.col("phone").rlike(r"^1\d{10}$"), "手机号格式必须为中国大陆 11 位号码"))
    add(make_issue(users, "users", "USR003", "有效性", "balance_cents", "warning", F.col("balance_cents").cast("long") < 0, "钱包余额不能为负数"))
    add(make_issue(users, "users", "USR004", "有效性", "status", "warning", ~F.col("status").isin("normal", "frozen"), "用户状态不在允许枚举中"))
    add(duplicate_issue(users, "users", "id", "USR005"))
    add(duplicate_issue(users, "users", "phone", "USR006"))

    # 站点表：位置、价格、名称、状态和来源标识。
    add(make_issue(stations, "stations", "STA001", "完整性", "name", "critical", F.col("name").isNull() | (F.trim("name") == ""), "站点名称不能为空"))
    add(make_issue(stations, "stations", "STA002", "有效性", "longitude", "critical", ~F.col("longitude").cast("double").between(113.6, 114.7), "经度不在深圳合理范围"))
    add(make_issue(stations, "stations", "STA003", "有效性", "latitude", "critical", ~F.col("latitude").cast("double").between(22.3, 23.0), "纬度不在深圳合理范围"))
    add(make_issue(stations, "stations", "STA004", "有效性", "price_cents_per_kwh", "critical", F.col("price_cents_per_kwh").cast("int") <= 0, "充电单价必须大于零"))
    add(make_issue(stations, "stations", "STA005", "有效性", "status", "warning", ~F.col("status").isin("online", "offline"), "站点状态不在允许枚举中"))
    add(duplicate_issue(stations, "stations", "id", "STA006"))

    # 充电桩表：功率、类型、状态、时间和站点外键。
    add(make_issue(piles, "charging_piles", "PIL001", "有效性", "power_kw", "critical", F.col("power_kw").cast("double") <= 0, "充电桩功率必须大于零"))
    add(make_issue(piles, "charging_piles", "PIL002", "有效性", "charge_type", "warning", ~F.col("charge_type").isin("fast", "slow"), "充电类型不在允许枚举中"))
    add(make_issue(piles, "charging_piles", "PIL003", "有效性", "status", "warning", ~F.col("status").isin("idle", "reserved", "charging", "fault", "offline", "disabled"), "充电桩状态不在允许枚举中"))
    add(make_issue(piles, "charging_piles", "PIL004", "有效性", "last_heartbeat_at", "warning", F.to_timestamp("last_heartbeat_at").isNull(), "心跳时间无法解析"))
    add(duplicate_issue(piles, "charging_piles", "pile_code", "PIL005"))
    orphan_piles = piles.join(stations.select(F.col("id").alias("valid_station_id")), piles.station_id == F.col("valid_station_id"), "left_anti")
    add(make_issue(orphan_piles, "charging_piles", "PIL006", "参照完整性", "station_id", "critical", F.lit(True), "所属站点不存在"))

    # 订单表：唯一性、外键、时间逻辑、数值范围与金额一致性。
    add(duplicate_issue(orders, "charging_orders", "order_no", "ORD001"))
    add(make_issue(orders, "charging_orders", "ORD002", "完整性", "started_at", "critical", F.to_timestamp("started_at").isNull(), "订单开始时间不能为空且必须可解析"))
    add(make_issue(orders, "charging_orders", "ORD003", "一致性", "stopped_at", "critical", F.to_timestamp("stopped_at") < F.to_timestamp("started_at"), "订单结束时间早于开始时间"))
    add(make_issue(orders, "charging_orders", "ORD004", "有效性", "duration_seconds", "critical", F.col("duration_seconds").cast("long") < 0, "充电时长不能为负数"))
    add(make_issue(orders, "charging_orders", "ORD005", "有效性", "energy_wh", "critical", F.col("energy_wh").cast("long") < 0, "充电量不能为负数"))
    add(make_issue(orders, "charging_orders", "ORD006", "异常值", "energy_wh", "warning", F.col("energy_wh").cast("long") > 500000, "单笔充电量超过合理上限"))
    add(make_issue(orders, "charging_orders", "ORD007", "有效性", "fee_cents", "critical", F.col("fee_cents").cast("long") < 0, "订单金额不能为负数"))
    add(make_issue(orders, "charging_orders", "ORD008", "有效性", "status", "warning", ~F.col("status").isin("charging", "completed", "fault_stopped", "cancelled"), "订单状态不在允许枚举中"))
    expected_fee = F.round(F.col("energy_wh").cast("double") / 1000 * F.col("unit_price_cents").cast("double"))
    add(make_issue(orders, "charging_orders", "ORD009", "一致性", "fee_cents", "warning", F.abs(F.col("fee_cents").cast("double") - expected_fee) > 2, "订单金额与电量乘单价不一致"))
    orphan_user_orders = orders.join(users.select(F.col("id").alias("valid_user_id")), orders.user_id == F.col("valid_user_id"), "left_anti")
    add(make_issue(orphan_user_orders, "charging_orders", "ORD010", "参照完整性", "user_id", "critical", F.lit(True), "订单用户不存在"))
    orphan_station_orders = orders.join(stations.select(F.col("id").alias("valid_station_id")), orders.station_id == F.col("valid_station_id"), "left_anti")
    add(make_issue(orphan_station_orders, "charging_orders", "ORD011", "参照完整性", "station_id", "critical", F.lit(True), "订单站点不存在"))
    orphan_pile_orders = orders.join(piles.select(F.col("id").alias("valid_pile_id")), orders.pile_id == F.col("valid_pile_id"), "left_anti")
    add(make_issue(orphan_pile_orders, "charging_orders", "ORD012", "参照完整性", "pile_id", "critical", F.lit(True), "订单充电桩不存在"))

    # 告警和天气数据。
    add(make_issue(alarms, "alarms", "ALM001", "完整性", "alarm_type", "critical", F.col("alarm_type").isNull() | (F.trim("alarm_type") == ""), "告警类型不能为空"))
    add(make_issue(alarms, "alarms", "ALM002", "有效性", "severity", "warning", ~F.col("severity").isin("info", "warning", "critical"), "告警级别不在允许枚举中"))
    add(make_issue(alarms, "alarms", "ALM003", "有效性", "occurred_at", "critical", F.to_timestamp("occurred_at").isNull(), "告警时间无法解析"))
    orphan_alarm_piles = alarms.join(piles.select(F.col("id").alias("valid_pile_id")), alarms.pile_id == F.col("valid_pile_id"), "left_anti")
    add(make_issue(orphan_alarm_piles, "alarms", "ALM004", "参照完整性", "pile_id", "critical", F.lit(True), "告警对应充电桩不存在"))
    add(make_issue(weather, "weather_daily", "WTH001", "有效性", "temperature_c", "warning", ~F.col("temperature_c").cast("double").between(-10, 50), "气温超出深圳合理范围"))
    add(make_issue(weather, "weather_daily", "WTH002", "有效性", "rainfall_mm", "warning", F.col("rainfall_mm").cast("double") < 0, "降雨量不能为负数"))
    add(duplicate_issue(weather, "weather_daily", "date", "WTH003"))

    all_issues = reduce(lambda left, right: left.unionByName(right), issues).cache()
    issue_count = all_issues.count()
    total_rows = sum(df.count() for df in tables.values())

    issue_output = QUALITY_ROOT / "issues"
    all_issues.write.mode("overwrite").parquet(local_uri(issue_output / "parquet"))
    all_issues.coalesce(1).write.mode("overwrite").option("header", True).csv(local_uri(issue_output / "csv"))

    grouped = (
        all_issues.groupBy("table_name", "rule_id", "category", "field_name", "severity", "problem")
        .count()
        .orderBy(F.desc("count"), "table_name", "rule_id")
        .collect()
    )
    summary = [row.asDict(recursive=True) for row in grouped]
    samples = [row.asDict(recursive=True) for row in all_issues.limit(100).collect()]
    report = {
        **current_run_metadata(),
        "total_rows_scanned": total_rows,
        "total_issues": issue_count,
        "issue_rate_percent": round(issue_count * 100 / max(total_rows, 1), 4),
        "table_row_counts": {name: df.count() for name, df in tables.items()},
        "rule_summary": summary,
        "samples": samples,
    }
    write_json(QUALITY_ROOT / "quality_report.json", report)
    write_json(QUALITY_ROOT / "quality_summary.json", summary)
    print(f"PySpark 数据质量扫描完成，共扫描 {total_rows} 行，发现 {issue_count} 个问题")
    for row in summary:
        print(f"{row['rule_id']} {row['table_name']}.{row['field_name']}: {row['count']}")
    spark.stop()


if __name__ == "__main__":
    main()
