"""Flask 数据服务与 Vue/ECharts 大屏静态站点。"""

from __future__ import annotations

import json
import os
import hashlib
from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify, render_template, request

try:
    from .api_contract import failure, is_stale, page_limit, station_ids, success
    from .auth import authenticated, tenant_scope
    from .audit import record
except ImportError:  # 支持 `python dashboard/app.py` 和独立文件测试加载。
    import sys

    dashboard_dir = str(Path(__file__).resolve().parent)
    if dashboard_dir not in sys.path:
        sys.path.insert(0, dashboard_dir)
    from api_contract import failure, is_stale, page_limit, station_ids, success
    from auth import authenticated, tenant_scope
    from audit import record


ROOT = Path(__file__).resolve().parents[1]
ADS_ROOT = Path(os.environ.get("CHARGING_ADS_EXPORT", ROOT / "data" / "exports" / "ads"))

app = Flask(__name__)

ADVANCED_FILES = {
    "model-comparison": "model_comparison.json", "drift": "drift_report.json",
    "scheduling": "scheduling_advice.json", "maintenance": "maintenance_advice.json",
    "expansion": "expansion_advice.json", "regulator": "regulator_summary.json",
}


def load_json(filename: str, default):
    path = ADS_ROOT / filename
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8"))


def json_export_response(filename: str, data):
    response = success(data, stale=is_stale(data))
    digest = hashlib.sha256(
        json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    response.set_etag(digest)
    return response.make_conditional(request)


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
        "lowerBound": float(
            prediction_value(row, "lowerBound", "lower_bound", default=0)
        ),
        "upperBound": float(
            prediction_value(
                row, "upperBound", "upper_bound",
                default=prediction_value(
                    row, "predictedSessions", "predicted_sessions", "prediction", default=0
                ),
            )
        ),
    }


@app.after_request
def add_headers(response):
    response.headers["Cache-Control"] = "no-store"
    return response


@app.errorhandler(json.JSONDecodeError)
@app.errorhandler(OSError)
def invalid_analytics_export(_error):
    return failure("analytics_data_invalid", "分析数据暂不可用", 503)


@app.before_request
def authorize_remote_api():
    if request.path.startswith("/api/") and not authenticated():
        return failure("unauthorized", "分析接口需要有效访问凭据", 401)


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
    configured_horizons = os.environ.get("CHARGING_FORECAST_HORIZONS", "1,6,24,48,72")
    try:
        allowed_horizons = {int(value) for value in configured_horizons.split(",")}
    except ValueError:
        allowed_horizons = {1, 6, 24, 48, 72}
    horizon, error = integer_query("horizonHours", allowed=allowed_horizons)
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


@app.get("/api/<name>")
def advanced(name):
    filename = ADVANCED_FILES.get(name)
    if not filename:
        return failure("not_found", "接口不存在", 404)
    try:
        requested_ids = station_ids()
        limit = page_limit()
    except ValueError as error:
        code = str(error)
        return failure(code, "站点查询范围无效", 400)
    data = load_json(filename, {})
    if requested_ids and isinstance(data, list):
        data = [row for row in data if row.get("stationId", row.get("station_id")) in requested_ids]
    if isinstance(data, list):
        data = data[:limit]
    record({"path": request.path, "result": "success", "requestId": request.headers.get("X-Request-Id", "")})
    return json_export_response(filename, data)


@app.get("/api/tenant/overview")
def tenant_overview():
    tenant, allowed = tenant_scope()
    if not allowed:
        return failure("forbidden", "不能访问其他租户的数据", 403)
    rows = load_json("tenant_overview.json", [])
    return success([row for row in rows if row.get("tenantId") == tenant])


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("DASHBOARD_PORT", "5000")), debug=False)
