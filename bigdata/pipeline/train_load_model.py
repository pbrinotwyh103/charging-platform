"""使用 Spark MLlib 训练站点充电负荷预测模型。"""

from __future__ import annotations

from datetime import datetime, timedelta
import argparse

from pyspark.ml.evaluation import RegressionEvaluator
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import RandomForestRegressor
from pyspark.ml.regression import LinearRegression, GBTRegressor
from pyspark.ml import Pipeline
from pyspark.sql import functions as F

from common import (
    EXPORT_ROOT,
    MODEL_ROOT,
    WAREHOUSE_ROOT,
    create_spark,
    ensure_local_directories,
    hdfs_path,
    local_uri,
    write_json,
)
from model_registry import ModelRegistry, select_candidate
from model_drift import build_drift_report
from incremental_features import publish_checkpoint
from run_manifest import current_run_metadata
import os


FEATURE_COLUMNS = [
    "station_id",
    "start_hour",
    "day_of_week",
    "is_weekend",
    "is_holiday",
    "temperature_c",
    "rainfall_mm",
    "price_cents_per_kwh",
    "pile_count",
    "fast_pile_count",
    "unavailable_pile_count",
]


def scenario_test() -> None:
    """Train a small real MLlib model and verify paired scenario directions."""
    spark = create_spark("charging-load-scenario-test")
    rows = []
    for repeat in range(30):
        for hour in range(24):
            for rain in (0.0, 8.0):
                peak = 1 if hour in {8, 9, 17, 18} else 0
                rows.append({"station_id": 1.0, "start_hour": float(hour), "day_of_week": float(repeat % 7 + 1),
                    "is_weekend": float(repeat % 7 in {5, 6}), "is_holiday": 0.0,
                    "temperature_c": 25.0, "rainfall_mm": rain, "price_cents_per_kwh": 150.0,
                    "pile_count": 20.0, "fast_pile_count": 10.0, "unavailable_pile_count": 0.0,
                    "label": 4.0 + peak * 12.0 - rain * 0.15})
    frame = spark.createDataFrame(rows)
    model = Pipeline(stages=[VectorAssembler(inputCols=FEATURE_COLUMNS, outputCol="features"),
                             RandomForestRegressor(numTrees=30, maxDepth=6, seed=20260915)]).fit(frame)
    paired = spark.createDataFrame([
        {**rows[0], "scenario": "offpeak", "start_hour": 3.0, "rainfall_mm": 0.0},
        {**rows[0], "scenario": "peak", "start_hour": 8.0, "rainfall_mm": 0.0},
        {**rows[0], "scenario": "rain", "start_hour": 8.0, "rainfall_mm": 8.0},
    ])
    values = {row["scenario"]: row["prediction"] for row in model.transform(paired).select("scenario", "prediction").collect()}
    assert values["peak"] > values["offpeak"], values
    assert values["rain"] <= values["peak"], values
    print("scenario directions passed", values)
    spark.stop()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario-test", action="store_true")
    args = parser.parse_args()
    if args.scenario_test:
        scenario_test()
        return
    ensure_local_directories()
    spark = create_spark("charging-load-prediction")
    spark.sparkContext.setLogLevel("WARN")

    source = spark.read.parquet(local_uri(WAREHOUSE_ROOT / "dws" / "dws_station_hour"))
    dataset = (
        source.select(
            "order_date",
            "station_name",
            *FEATURE_COLUMNS,
            F.col("session_count").cast("double").alias("label"),
        )
        .na.fill(
            {
                "temperature_c": 25.0,
                "rainfall_mm": 0.0,
                "is_holiday": 0,
                "unavailable_pile_count": 0,
            }
        )
        .cache()
    )

    date_bounds = dataset.agg(F.min("order_date"), F.max("order_date")).first()
    min_date, max_date = date_bounds[0], date_bounds[1]
    if min_date is None or max_date is None:
        raise RuntimeError("没有足够的 DWS 小时负荷数据用于训练")
    cutoff = max_date - timedelta(days=14)
    train = dataset.filter(F.col("order_date") < F.lit(cutoff))
    test = dataset.filter(F.col("order_date") >= F.lit(cutoff))
    if train.count() < 100 or test.count() < 20:
        train, test = dataset.randomSplit([0.8, 0.2], seed=20260915)

    assembler = VectorAssembler(inputCols=FEATURE_COLUMNS, outputCol="features")
    regressors = {
        "random_forest": RandomForestRegressor(featuresCol="features", labelCol="label", predictionCol="prediction", numTrees=60, maxDepth=8, minInstancesPerNode=3, subsamplingRate=0.85, seed=20260915),
        "linear_regression": LinearRegression(featuresCol="features", labelCol="label", predictionCol="prediction", maxIter=80, regParam=0.1),
        "gradient_boosted_trees": GBTRegressor(featuresCol="features", labelCol="label", predictionCol="prediction", maxIter=40, maxDepth=6, seed=20260915),
    }
    trained = {}
    candidates = []
    evaluators = {name: RegressionEvaluator(labelCol="label", predictionCol="prediction", metricName=name)
                  for name in ("rmse", "mae", "r2")}
    for name, regressor in regressors.items():
        candidate_model = Pipeline(stages=[assembler, regressor]).fit(train)
        candidate_predictions = candidate_model.transform(test).cache()
        candidate_metrics = {metric: round(evaluator.evaluate(candidate_predictions), 4)
                             for metric, evaluator in evaluators.items()}
        candidate_metrics.update({"name": name, "constraintsPassed":
            candidate_predictions.filter(F.isnan("prediction") | F.col("prediction").isNull()).limit(1).count() == 0})
        candidates.append(candidate_metrics)
        trained[name] = (candidate_model, candidate_predictions)
    selected = select_candidate(candidates)
    model, predictions = trained[selected["name"]]

    metrics = {
        "algorithm": selected["name"],
        "target": "station hourly charging session count",
        "train_start": min_date.isoformat(),
        "test_end": max_date.isoformat(),
        "train_rows": train.count(),
        "test_rows": test.count(),
        "rmse": selected["rmse"], "mae": selected["mae"], "r2": selected["r2"],
        "candidates": candidates,
    }
    fitted_model = model.stages[-1]
    if hasattr(fitted_model, "featureImportances"):
        importances = fitted_model.featureImportances.toArray()
    else:
        coefficients = [abs(float(value)) for value in fitted_model.coefficients]
        total = sum(coefficients) or 1.0
        importances = [value / total for value in coefficients]
    metrics["feature_importance"] = [
        {"feature": name, "importance": round(float(value), 6)}
        for name, value in sorted(
            zip(FEATURE_COLUMNS, importances),
            key=lambda pair: pair[1],
            reverse=True,
        )
    ]

    model.write().overwrite().save(local_uri(MODEL_ROOT))
    model.write().overwrite().save(hdfs_path("models", "station_load_rf"))

    metadata = current_run_metadata()
    model_version = f"{selected['name']}-{metadata['batch_id']}"
    metrics.update({"modelVersion": model_version, "featureVersion": "station-hour-v2", **metadata})
    registry = ModelRegistry(MODEL_ROOT.parent / "registry")
    registry.register(metrics)
    registry.promote(model_version)

    # 选择历史订单量最高的 20 个站点，预测配置的未来时段负荷。
    station_profiles = (
        source.groupBy("station_id", "station_name")
        .agg(
            F.sum("session_count").alias("historical_sessions"),
            F.avg("price_cents_per_kwh").alias("price_cents_per_kwh"),
            F.max("pile_count").alias("pile_count"),
            F.max("fast_pile_count").alias("fast_pile_count"),
            F.max("unavailable_pile_count").alias("unavailable_pile_count"),
            F.avg("temperature_c").alias("temperature_c"),
            F.avg("rainfall_mm").alias("rainfall_mm"),
        )
        .orderBy(F.desc("historical_sessions"))
        .limit(20)
        .collect()
    )
    base_time = datetime.combine(max_date, datetime.min.time()) + timedelta(hours=23)
    future_rows = []
    for station in station_profiles:
        horizons = sorted({int(value) for value in os.environ.get("CHARGING_FORECAST_HORIZONS", "1,6,24,48,72").split(",") if int(value) > 0})
        for horizon in horizons:
            target = base_time + timedelta(hours=horizon)
            day_of_week = ((target.weekday() + 1) % 7) + 1  # Spark: 周日=1
            future_rows.append(
                {
                    "station_id": float(station["station_id"]),
                    "station_name": station["station_name"],
                    "forecast_horizon_hours": horizon,
                    "forecast_time": target.isoformat(),
                    "start_hour": float(target.hour),
                    "day_of_week": float(day_of_week),
                    "is_weekend": float(1 if target.weekday() >= 5 else 0),
                    "is_holiday": float(1 if target.weekday() >= 5 else 0),
                    "temperature_c": float(station["temperature_c"] or 25.0),
                    "rainfall_mm": float(station["rainfall_mm"] or 0.0),
                    "price_cents_per_kwh": float(station["price_cents_per_kwh"] or 150.0),
                    "pile_count": float(station["pile_count"] or 1),
                    "fast_pile_count": float(station["fast_pile_count"] or 0),
                    "unavailable_pile_count": float(station["unavailable_pile_count"] or 0),
                }
            )

    future_df = spark.createDataFrame(future_rows)
    residual_radius = predictions.withColumn("absolute_error", F.abs(F.col("prediction") - F.col("label"))).approxQuantile("absolute_error", [0.95], 0.01)[0]
    forecast = (
        model.transform(future_df)
        .withColumn("predicted_sessions", F.round(F.greatest(F.col("prediction"), F.lit(0.0)), 2))
        .withColumn(
            "predicted_idle_piles",
            F.greatest(
                F.col("pile_count").cast("int")
                - F.ceil(F.col("predicted_sessions")).cast("int")
                - F.col("unavailable_pile_count").cast("int"),
                F.lit(0),
            ),
        )
        .withColumn("lower_bound", F.round(F.greatest(F.col("predicted_sessions") - F.lit(residual_radius), F.lit(0.0)), 2))
        .withColumn("upper_bound", F.round(F.col("predicted_sessions") + F.lit(residual_radius), 2))
        .withColumn("predicted_peak_period", F.concat(F.lpad(F.hour("forecast_time"), 2, "0"), F.lit(":00-"),
                                                       F.lpad((F.hour("forecast_time") + 1) % 24, 2, "0"), F.lit(":00")))
        .withColumn("model_version", F.lit(model_version))
        .withColumn("generated_at", F.lit(metadata["generated_at"]))
        .select(
            F.col("station_id").cast("long").alias("station_id"),
            "station_name",
            "forecast_horizon_hours",
            "forecast_time",
            "predicted_sessions",
            "predicted_idle_piles",
            "lower_bound", "upper_bound", "predicted_peak_period", "model_version", "generated_at",
            F.col("pile_count").cast("int").alias("pile_count"),
        )
        .orderBy("forecast_horizon_hours", F.desc("predicted_sessions"))
    )
    forecast.write.mode("overwrite").parquet(local_uri(WAREHOUSE_ROOT / "ads" / "ads_load_prediction"))
    forecast.write.mode("overwrite").parquet(hdfs_path("ads", "ads_load_prediction"))

    forecast_rows = [row.asDict(recursive=True) for row in forecast.collect()]
    write_json(EXPORT_ROOT / "predictions.json", forecast_rows)
    write_json(EXPORT_ROOT / "model_metrics.json", metrics)
    write_json(EXPORT_ROOT / "model_comparison.json", {"selected": selected["name"],
        "candidates": candidates, "generatedAt": metadata["generated_at"], "modelVersion": model_version})
    hour_bins = []
    for frame in (train, test):
        hour_bins.append([frame.filter((F.col("start_hour") >= start) & (F.col("start_hour") < start + 6)).count()
                          for start in (0, 6, 12, 18)])
    drift = build_drift_report({"startHour": (hour_bins[0], hour_bins[1])},
                               float(selected["mae"]), float(selected["mae"]))
    drift.update({"generatedAt": metadata["generated_at"], "modelVersion": model_version})
    write_json(EXPORT_ROOT / "drift_report.json", drift)
    scheduling = []
    maintenance = []
    expansion = []
    for row in forecast_rows:
        station_id = int(row["station_id"])
        horizon = int(row["forecast_horizon_hours"])
        stable = f"{station_id}-{horizon}-{model_version}"
        base = {"stationId": station_id, "stationName": row["station_name"],
                "generatedAt": metadata["generated_at"], "modelVersion": model_version,
                "stale": False, "readOnly": True}
        scheduling.append({**base, "adviceId": f"schedule-{stable}", "kind": "scheduling",
            "severity": "severe" if row["predicted_idle_piles"] == 0 else "attention" if row["predicted_idle_piles"] <= 2 else "normal",
            "reasons": [f"未来 {horizon} 小时预测会话 {row['predicted_sessions']}", f"预计空闲桩 {row['predicted_idle_piles']}"]})
        if horizon == 24:
            maintenance.append({**base, "adviceId": f"maintenance-{stable}", "kind": "maintenance",
                "severity": "attention" if row["predicted_idle_piles"] <= 2 else "normal",
                "reasons": ["结合不可用设备与预测负荷安排人工巡检"]})
            expansion.append({**base, "adviceId": f"expansion-{stable}", "kind": "expansion",
                "severity": "attention" if row["predicted_sessions"] >= row["pile_count"] * 0.8 else "normal",
                "reasons": ["持续利用率与排队压力需结合多批次趋势复核"]})
    write_json(EXPORT_ROOT / "scheduling_advice.json", scheduling)
    write_json(EXPORT_ROOT / "maintenance_advice.json", maintenance)
    write_json(EXPORT_ROOT / "expansion_advice.json", expansion)
    write_json(EXPORT_ROOT / "regulator_summary.json", {"stationCount": len(station_profiles),
        "predictionCount": len(forecast_rows), "generatedAt": metadata["generated_at"],
        "dataVersion": metadata["data_version"], "containsPersonalData": False})
    publish_checkpoint(WAREHOUSE_ROOT / "checkpoints" / "station_features.json",
        [EXPORT_ROOT / "predictions.json", EXPORT_ROOT / "model_metrics.json", EXPORT_ROOT / "drift_report.json"],
        max_date.isoformat())
    print(
        "Spark MLlib 训练完成: "
        f"RMSE={metrics['rmse']}, MAE={metrics['mae']}, R2={metrics['r2']}"
    )
    print(f"模型已保存: {MODEL_ROOT}")
    spark.stop()


if __name__ == "__main__":
    main()
