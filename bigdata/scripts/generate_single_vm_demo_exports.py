"""Generate deterministic ADS exports for the single-VM classroom demo.

The production pipeline remains the PySpark ODS -> DWD -> DWS -> ADS flow.  This
small fallback lets a freshly prepared BitDev VM display every dashboard view
immediately when the full Spark runtime is unavailable during a presentation.
"""

from __future__ import annotations

import json
import math
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path


def write(root: Path, name: str, value) -> None:
    root.mkdir(parents=True, exist_ok=True)
    (root / name).write_text(
        json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8"
    )


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: generate_single_vm_demo_exports.py OUTPUT_DIR")

    root = Path(sys.argv[1]).resolve()
    now = datetime.now(timezone(timedelta(hours=8))).replace(microsecond=0)
    generated_at = now.isoformat()
    version = f"single-vm-demo-{now:%Y%m%d}"

    revenues = []
    total_revenue = 0.0
    total_orders = 0
    total_energy = 0.0
    for offset in range(29, -1, -1):
        date = (now - timedelta(days=offset)).date().isoformat()
        day_index = 29 - offset
        orders = 2450 + day_index * 31 + (day_index % 7) * 95
        revenue = round(orders * (31.5 + (day_index % 5) * 1.7), 2)
        energy = round(revenue / 1.52, 2)
        revenues.append(
            {
                "date": date,
                "revenue_yuan": revenue,
                "orders": orders,
                "energy_kwh": energy,
            }
        )
        total_revenue += revenue
        total_orders += orders
        total_energy += energy

    stations = [
        {
            "station_id": index,
            "station_name": f"深圳-{1000 + index}",
            "revenue_yuan": round(
                168000 - index * 7350 + (index % 3) * 2100, 2
            ),
            "orders": 4600 - index * 170,
        }
        for index in range(1, 11)
    ]
    hourly = []
    for hour in range(24):
        morning = 8.5 * math.exp(-((hour - 8) ** 2) / 8)
        evening = 11.0 * math.exp(-((hour - 18) ** 2) / 10)
        hourly.append(
            {"hour": hour, "avg_sessions": round(3.2 + morning + evening, 2)}
        )

    pile_status = [
        {"status": "空闲", "count": 1868},
        {"status": "充电中", "count": 721},
        {"status": "离线", "count": 196},
        {"status": "故障", "count": 104},
    ]
    district_values = {
        "南山区": 685000,
        "福田区": 618000,
        "宝安区": 562000,
        "龙岗区": 498000,
        "罗湖区": 421000,
        "龙华区": 389000,
    }
    districts = [
        {"district": name, "revenue_yuan": value, "orders": int(value / 34)}
        for name, value in district_values.items()
    ]

    predictions = []
    for horizon in (1, 6, 24, 48, 72):
        for index in range(1, 11):
            pile_count = 28 + (index % 5) * 4
            predicted = round(7.0 + index * 0.9 + horizon * 0.08, 2)
            available = max(0, pile_count - int(round(predicted * 1.5)))
            predictions.append(
                {
                    "station_id": index,
                    "station_name": f"深圳-{1000 + index}",
                    "forecast_horizon_hours": horizon,
                    "forecast_time": (now + timedelta(hours=horizon)).isoformat(),
                    "predicted_sessions": predicted,
                    "predicted_idle_piles": available,
                    "lower_bound": round(max(0, predicted - 1.4), 2),
                    "upper_bound": round(predicted + 1.4, 2),
                    "predicted_peak_period": (
                        f"{(now.hour + horizon) % 24:02d}:00-"
                        f"{(now.hour + horizon + 1) % 24:02d}:00"
                    ),
                    "model_version": "gradient_boosted_trees-demo-v1",
                    "generated_at": generated_at,
                    "pile_count": pile_count,
                }
            )

    model_candidates = [
        {
            "name": "random_forest",
            "rmse": 0.6921,
            "mae": 0.4782,
            "r2": 0.1948,
            "constraintsPassed": True,
        },
        {
            "name": "linear_regression",
            "rmse": 0.8114,
            "mae": 0.5691,
            "r2": 0.1024,
            "constraintsPassed": True,
        },
        {
            "name": "gradient_boosted_trees",
            "rmse": 0.6672,
            "mae": 0.4531,
            "r2": 0.2106,
            "constraintsPassed": True,
        },
    ]
    model_version = "gradient_boosted_trees-demo-v1"
    model_metrics = {
        "algorithm": "gradient_boosted_trees",
        "rmse": 0.6672,
        "mae": 0.4531,
        "r2": 0.2106,
        "train_rows": 77120,
        "test_rows": 19280,
        "modelVersion": model_version,
        "featureVersion": "station-hour-v2",
        "generatedAt": generated_at,
    }

    advice = []
    maintenance = []
    expansion = []
    for row in predictions:
        if row["forecast_horizon_hours"] != 24:
            continue
        available = row["predicted_idle_piles"]
        severity = (
            "severe" if available == 0 else "attention" if available <= 5 else "normal"
        )
        common = {
            "stationId": row["station_id"],
            "stationName": row["station_name"],
            "generatedAt": generated_at,
            "modelVersion": model_version,
            "stale": False,
            "readOnly": True,
            "severity": severity,
        }
        advice.append(
            {
                **common,
                "adviceId": f"schedule-{row['station_id']}",
                "kind": "scheduling",
                "reasons": [
                    f"未来24小时预测会话 {row['predicted_sessions']}",
                    f"预计空闲桩 {available}",
                ],
            }
        )
        maintenance.append(
            {
                **common,
                "adviceId": f"maintenance-{row['station_id']}",
                "kind": "maintenance",
                "reasons": ["结合设备告警与预测负荷安排人工巡检"],
            }
        )
        expansion.append(
            {
                **common,
                "adviceId": f"expansion-{row['station_id']}",
                "kind": "expansion",
                "reasons": ["持续高负荷站点建议复核扩容计划"],
            }
        )

    write(
        root,
        "overview.json",
        [
            {
                "total_revenue_yuan": round(total_revenue, 2),
                "total_energy_kwh": round(total_energy, 2),
                "order_count": total_orders,
                "completion_rate_percent": 96.8,
                "station_count": 296,
                "pile_count": 2889,
                "user_count": 1000,
            }
        ],
    )
    write(root, "revenue_trend.json", revenues)
    write(root, "station_rank.json", stations)
    write(root, "hourly_load.json", hourly)
    write(root, "pile_status.json", pile_status)
    write(
        root,
        "alarm_distribution.json",
        [
            {"level": "提示", "count": 74},
            {"level": "警告", "count": 28},
            {"level": "严重", "count": 9},
        ],
    )
    write(root, "district_metrics.json", districts)
    write(
        root,
        "quality.json",
        {
            "total_rows_scanned": 105646,
            "total_issues": 46,
            "issue_rate_percent": 0.0435,
            "rule_summary": [
                {"category": "完整性", "count": 9},
                {"category": "唯一性", "count": 7},
                {"category": "有效性", "count": 13},
                {"category": "参照完整性", "count": 8},
                {"category": "业务一致性", "count": 6},
                {"category": "异常值", "count": 3},
            ],
        },
    )
    write(root, "predictions.json", predictions)
    write(root, "model_metrics.json", model_metrics)
    write(
        root,
        "model_comparison.json",
        {
            "selected": "gradient_boosted_trees",
            "candidates": model_candidates,
            "generatedAt": generated_at,
            "modelVersion": model_version,
        },
    )
    write(
        root,
        "drift_report.json",
        {
            "level": "normal",
            "score": 0.072,
            "errorRatio": 1.03,
            "featureScores": {
                "startHour": 0.052,
                "rainfall": 0.081,
                "temperature": 0.066,
            },
            "generatedAt": generated_at,
            "modelVersion": model_version,
        },
    )
    write(root, "scheduling_advice.json", advice)
    write(root, "maintenance_advice.json", maintenance)
    write(root, "expansion_advice.json", expansion)
    write(
        root,
        "regulator_summary.json",
        {
            "stationCount": 296,
            "predictionCount": len(predictions),
            "generatedAt": generated_at,
            "dataVersion": version,
            "containsPersonalData": False,
        },
    )
    print(f"Generated {len(list(root.glob('*.json')))} ADS exports in {root}")


if __name__ == "__main__":
    main()
