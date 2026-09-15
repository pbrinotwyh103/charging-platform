import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


APP_PATH = Path(__file__).resolve().parents[1] / "dashboard" / "app.py"


class DashboardApiTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        spec = importlib.util.spec_from_file_location("charging_dashboard_app", APP_PATH)
        self.dashboard = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(self.dashboard)
        self.dashboard.ADS_ROOT = Path(self.directory.name)
        self.dashboard.app.config.update(TESTING=True)
        self.client = self.dashboard.app.test_client()

    def tearDown(self):
        self.directory.cleanup()

    def write_json(self, filename, value):
        (self.dashboard.ADS_ROOT / filename).write_text(
            json.dumps(value), encoding="utf-8"
        )

    def test_predictions_filters_and_normalizes_query(self):
        self.write_json(
            "predictions.json",
            [
                {
                    "station_id": 1,
                    "horizon_hours": 1,
                    "prediction": 9.5,
                    "predicted_available_piles": 2,
                    "generated_at": "2026-09-15T10:00:00+08:00",
                    "model_version": "rf-1",
                },
                {"station_id": 2, "horizon_hours": 6, "prediction": 4},
            ],
        )
        response = self.client.get("/api/predictions?horizonHours=1&stationId=1")
        self.assertEqual(response.status_code, 200)
        self.assertEqual(
            response.get_json(),
            [{
                "stationId": 1,
                "stationName": "",
                "horizonHours": 1,
                "predictedSessions": 9.5,
                "predictedAvailablePiles": 2,
                "totalPiles": 0,
                "forecastTime": "",
                "generatedAt": "2026-09-15T10:00:00+08:00",
                "modelVersion": "rf-1",
            }],
        )

    def test_predictions_rejects_invalid_query(self):
        cases = [
            ("horizonHours=2", "invalid_horizon"),
            ("horizonHours=abc", "invalid_horizon"),
            ("stationId=0", "invalid_station_id"),
            ("stationId=abc", "invalid_station_id"),
        ]
        for query, error in cases:
            with self.subTest(query=query):
                response = self.client.get(f"/api/predictions?{query}")
                self.assertEqual(response.status_code, 400)
                self.assertEqual(
                    response.get_json(),
                    {"error": error, "message": "请求参数无效"},
                )

    def test_health_does_not_expose_absolute_ads_path(self):
        payload = self.client.get("/api/health").get_json()
        self.assertEqual(payload["status"], "ok")
        self.assertNotIn("ads_root", payload)
        self.assertFalse(payload["adsAvailable"])


if __name__ == "__main__":
    unittest.main()
