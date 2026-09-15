"""使用 Spark MLlib 训练站点充电负荷预测模型。"""

from __future__ import annotations

from datetime import datetime, timedelta

from pyspark.ml.evaluation import RegressionEvaluator
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import RandomForestRegressor
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


def main() -> None:
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
    regressor = RandomForestRegressor(
        featuresCol="features",
        labelCol="label",
        predictionCol="prediction",
        numTrees=60,
        maxDepth=8,
        minInstancesPerNode=3,
        subsamplingRate=0.85,
        seed=20260915,
    )
    pipeline = Pipeline(stages=[assembler, regressor])
    model = pipeline.fit(train)
    predictions = model.transform(test).cache()

    metrics = {
        "algorithm": "Spark MLlib RandomForestRegressor",
        "target": "station hourly charging session count",
        "train_start": min_date.isoformat(),
        "test_end": max_date.isoformat(),
        "train_rows": train.count(),
        "test_rows": test.count(),
        "rmse": round(RegressionEvaluator(labelCol="label", predictionCol="prediction", metricName="rmse").evaluate(predictions), 4),
        "mae": round(RegressionEvaluator(labelCol="label", predictionCol="prediction", metricName="mae").evaluate(predictions), 4),
        "r2": round(RegressionEvaluator(labelCol="label", predictionCol="prediction", metricName="r2").evaluate(predictions), 4),
    }
    rf_model = model.stages[-1]
    metrics["feature_importance"] = [
        {"feature": name, "importance": round(float(value), 6)}
        for name, value in sorted(
            zip(FEATURE_COLUMNS, rf_model.featureImportances.toArray()),
            key=lambda pair: pair[1],
            reverse=True,
        )
    ]

    model.write().overwrite().save(local_uri(MODEL_ROOT))
    model.write().overwrite().save(hdfs_path("models", "station_load_rf"))

    # 选择历史订单量最高的 20 个站点，预测未来 1、6、24 小时负荷。
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
        for horizon in (1, 6, 24):
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
        .select(
            F.col("station_id").cast("long").alias("station_id"),
            "station_name",
            "forecast_horizon_hours",
            "forecast_time",
            "predicted_sessions",
            "predicted_idle_piles",
            F.col("pile_count").cast("int").alias("pile_count"),
        )
        .orderBy("forecast_horizon_hours", F.desc("predicted_sessions"))
    )
    forecast.write.mode("overwrite").parquet(local_uri(WAREHOUSE_ROOT / "ads" / "ads_load_prediction"))
    forecast.write.mode("overwrite").parquet(hdfs_path("ads", "ads_load_prediction"))

    forecast_rows = [row.asDict(recursive=True) for row in forecast.collect()]
    write_json(EXPORT_ROOT / "predictions.json", forecast_rows)
    write_json(EXPORT_ROOT / "model_metrics.json", metrics)
    print(
        "Spark MLlib 训练完成: "
        f"RMSE={metrics['rmse']}, MAE={metrics['mae']}, R2={metrics['r2']}"
    )
    print(f"模型已保存: {MODEL_ROOT}")
    spark.stop()


if __name__ == "__main__":
    main()
