import json
import os
import tempfile
import unittest
from pathlib import Path


class DashboardCompleteApiTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        os.environ["CHARGING_ADS_EXPORT"] = str(self.root)
        os.environ["CHARGING_API_KEY"] = "test-secret"
        from bigdata.dashboard import app as dashboard
        dashboard.ADS_ROOT = self.root
        dashboard.app.config.update(TESTING=True)
        self.client = dashboard.app.test_client()

    def tearDown(self):
        self.directory.cleanup()
        os.environ.pop("CHARGING_API_KEY", None)

    def write(self, name, value):
        (self.root / name).write_text(json.dumps(value), "utf-8")

    def test_remote_api_requires_key_and_accepts_configured_key(self):
        denied = self.client.get("/api/drift", environ_base={"REMOTE_ADDR": "10.0.0.8"})
        self.assertEqual(denied.status_code, 401)
        self.assertEqual(denied.get_json()["error"], "unauthorized")
        allowed = self.client.get("/api/drift", headers={"X-Analytics-Key": "test-secret"},
                                  environ_base={"REMOTE_ADDR": "10.0.0.8"})
        self.assertEqual(allowed.status_code, 200)

    def test_advanced_endpoint_returns_uniform_versioned_envelope(self):
        self.write("drift_report.json", {"level": "attention", "score": 0.18,
                                          "generatedAt": "2026-09-15T12:00:00Z"})
        response = self.client.get("/api/drift")
        body = response.get_json()
        self.assertEqual(body["data"]["level"], "attention")
        self.assertIn("requestId", body["meta"])
        self.assertEqual(body["meta"]["generatedAt"], "2026-09-15T12:00:00Z")

    def test_tenant_scope_cannot_be_expanded(self):
        response = self.client.get("/api/tenant/overview?tenantId=other",
                                   headers={"X-Tenant-Id": "tenant-a"})
        self.assertEqual(response.status_code, 403)
        self.assertEqual(response.get_json()["error"], "forbidden")

    def test_station_query_limit_is_enforced(self):
        ids = ",".join(str(value) for value in range(1, 102))
        response = self.client.get(f"/api/scheduling?stationIds={ids}")
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.get_json()["error"], "too_many_station_ids")

    def test_malformed_export_returns_sanitized_error(self):
        (self.root / "drift_report.json").write_text("{broken", "utf-8")
        response = self.client.get("/api/drift")
        self.assertEqual(response.status_code, 503)
        body = response.get_json()
        self.assertEqual(body["error"], "analytics_data_invalid")
        self.assertNotIn(str(self.root), json.dumps(body))

    def test_prediction_supports_long_horizon_and_interval(self):
        self.write("predictions.json", [{
            "station_id": 3, "horizon_hours": 72, "prediction": 12,
            "lower_bound": 8, "upper_bound": 16,
        }])
        response = self.client.get("/api/predictions?horizonHours=72")
        self.assertEqual(response.status_code, 200)
        row = response.get_json()[0]
        self.assertEqual((row["lowerBound"], row["upperBound"]), (8.0, 16.0))

    def test_etag_supports_conditional_request(self):
        self.write("drift_report.json", {"level": "normal"})
        response = self.client.get("/api/drift")
        self.assertIn("ETag", response.headers)
        cached = self.client.get("/api/drift", headers={"If-None-Match": response.headers["ETag"]})
        self.assertEqual(cached.status_code, 304)

    def test_page_limit_and_stale_metadata(self):
        invalid = self.client.get("/api/scheduling?limit=201")
        self.assertEqual(invalid.status_code, 400)
        self.assertEqual(invalid.get_json()["error"], "invalid_limit")
        self.write("drift_report.json", {
            "level": "normal", "generatedAt": "2000-01-01T00:00:00Z"
        })
        self.assertTrue(self.client.get("/api/drift").get_json()["meta"]["stale"])


if __name__ == "__main__":
    unittest.main()
