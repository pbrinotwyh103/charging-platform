"""Flask 数据服务与 Vue/ECharts 大屏静态站点。"""

from __future__ import annotations

import json
import os
from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify, render_template


ROOT = Path(__file__).resolve().parents[1]
ADS_ROOT = Path(os.environ.get("CHARGING_ADS_EXPORT", ROOT / "data" / "exports" / "ads"))

app = Flask(__name__)


def load_json(filename: str, default):
    path = ADS_ROOT / filename
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8"))


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
            "ads_root": str(ADS_ROOT),
            "generated_at": datetime.now().isoformat(timespec="seconds"),
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
    return jsonify(load_json("predictions.json", []))


@app.get("/api/model-metrics")
def model_metrics():
    return jsonify(load_json("model_metrics.json", {}))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("DASHBOARD_PORT", "5000")), debug=False)

