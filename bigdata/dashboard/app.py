"""Flask 数据服务与 Vue/ECharts 大屏静态站点。"""

from __future__ import annotations

import json
import os
from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify, render_template, request


ROOT = Path(__file__).resolve().parents[1]
ADS_ROOT = Path(os.environ.get("CHARGING_ADS_EXPORT", ROOT / "data" / "exports" / "ads"))

app = Flask(__name__)


def load_json(filename: str, default):
    path = ADS_ROOT / filename
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8"))


def error_response(code: str):
    return jsonify({"error": code, "message": "请求参数无效"}), 400


def integer_query(name: str, allowed=None, minimum=None):
    raw = request.args.get(name)
    if raw is None:
        return None, None
    try:
        value = int(raw)
    except ValueError:
        return None, f"invalid_{name}"
    if (allowed is not None and value not in allowed) or (
        minimum is not None and value < minimum
    ):
        return None, f"invalid_{name}"
    return value, None


def prediction_value(row, *names, default=None):
    for name in names:
        if name in row and row[name] is not None:
            return row[name]
    return default


def normalized_prediction(row):
    station_id = prediction_value(row, "stationId", "station_id")
    horizon = prediction_value(
        row, "horizonHours", "horizon_hours", "forecast_horizon_hours"
    )
    return {
        "stationId": int(station_id),
        "stationName": str(
            prediction_value(row, "stationName", "station_name", default="")
        ),
        "horizonHours": int(horizon),
        "predictedSessions": float(
            prediction_value(
                row, "predictedSessions", "predicted_sessions", "prediction", default=0
            )
        ),
        "predictedAvailablePiles": max(
            0,
            int(
                prediction_value(
                    row,
                    "predictedAvailablePiles",
                    "predicted_available_piles",
                    "predicted_idle_piles",
                    default=0,
                )
            ),
        ),
        "totalPiles": max(
            0, int(prediction_value(row, "totalPiles", "pile_count", default=0))
        ),
        "forecastTime": str(
            prediction_value(row, "forecastTime", "forecast_time", default="")
        ),
        "generatedAt": str(
            prediction_value(row, "generatedAt", "generated_at", default="")
        ),
        "modelVersion": str(
            prediction_value(row, "modelVersion", "model_version", default="unknown")
        ),
    }


@app.after_request
def add_headers(response):
    response.headers["Cache-Control"] = "no-store"
    return response


@app.get("/")
def index():
    return render_template("index.html")


@app.get("/api/health")
def health():
    return jsonify(
        {
            "status": "ok",
            "service": "charging-bigdata-dashboard",
            "adsAvailable": (ADS_ROOT / "predictions.json").is_file(),
            "generatedAt": datetime.now().astimezone().isoformat(timespec="seconds"),
        }
    )


@app.get("/api/overview")
def overview():
    data = load_json("overview.json", [{}])
    return jsonify(data[0] if data else {})


@app.get("/api/revenue-trend")
def revenue_trend():
    return jsonify(load_json("revenue_trend.json", []))


@app.get("/api/station-rank")
def station_rank():
    return jsonify(load_json("station_rank.json", []))


@app.get("/api/hourly-load")
def hourly_load():
    return jsonify(load_json("hourly_load.json", []))


@app.get("/api/pile-status")
def pile_status():
    return jsonify(load_json("pile_status.json", []))


@app.get("/api/alarm-distribution")
def alarm_distribution():
    return jsonify(load_json("alarm_distribution.json", []))


@app.get("/api/district-metrics")
def district_metrics():
    return jsonify(load_json("district_metrics.json", []))


@app.get("/api/quality")
def quality():
    return jsonify(load_json("quality.json", {}))


@app.get("/api/predictions")
def predictions():
    horizon, error = integer_query("horizonHours", allowed={1, 6, 24})
    if error:
        return error_response("invalid_horizon")
    station_id, error = integer_query("stationId", minimum=1)
    if error:
        return error_response("invalid_station_id")
    rows = []
    for raw in load_json("predictions.json", []):
        try:
            row = normalized_prediction(raw)
        except (TypeError, ValueError):
            continue
        if horizon is not None and row["horizonHours"] != horizon:
            continue
        if station_id is not None and row["stationId"] != station_id:
            continue
        rows.append(row)
    return jsonify(rows)


@app.get("/api/model-metrics")
def model_metrics():
    return jsonify(load_json("model_metrics.json", {}))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("DASHBOARD_PORT", "5000")), debug=False)
