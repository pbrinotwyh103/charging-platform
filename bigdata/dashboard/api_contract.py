"""Stable response and query validation helpers for analytics APIs."""

import os
from datetime import datetime, timezone
from uuid import uuid4

from flask import jsonify, request


def request_id() -> str:
    return request.headers.get("X-Request-Id", "").strip()[:128] or uuid4().hex


def success(data, *, stale=False):
    generated = data.get("generatedAt", data.get("generated_at", "")) if isinstance(data, dict) else ""
    return jsonify({"data": data, "meta": {"requestId": request_id(), "stale": stale,
                    "generatedAt": generated or datetime.now(timezone.utc).isoformat()}})


def failure(error: str, message: str, status: int):
    return jsonify({"error": error, "message": message, "requestId": request_id()}), status


def station_ids(maximum=100):
    raw = request.args.get("stationIds", "").strip()
    if not raw:
        return []
    parts = raw.split(",")
    if len(parts) > maximum:
        raise ValueError("too_many_station_ids")
    try:
        values = [int(part) for part in parts]
    except ValueError as error:
        raise ValueError("invalid_station_ids") from error
    if any(value <= 0 for value in values):
        raise ValueError("invalid_station_ids")
    return values


def page_limit(default=200, maximum=200):
    raw = request.args.get("limit", str(default))
    try:
        value = int(raw)
    except ValueError as error:
        raise ValueError("invalid_limit") from error
    if value < 1 or value > maximum:
        raise ValueError("invalid_limit")
    return value


def is_stale(data) -> bool:
    if not isinstance(data, dict):
        return False
    raw = data.get("generatedAt", data.get("generated_at", ""))
    if not raw:
        return False
    try:
        generated = datetime.fromisoformat(str(raw).replace("Z", "+00:00"))
        if generated.tzinfo is None:
            generated = generated.replace(tzinfo=timezone.utc)
        maximum_age = int(os.environ.get("CHARGING_ANALYTICS_MAX_AGE_SECONDS", "86400"))
        return (datetime.now(timezone.utc) - generated.astimezone(timezone.utc)).total_seconds() > maximum_age
    except (TypeError, ValueError):
        return True
