import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / "dashboard"


class DashboardAssetsTest(unittest.TestCase):
    def test_dashboard_is_offline_and_has_all_views(self):
        html = (ROOT / "templates" / "index.html").read_text("utf-8")
        self.assertNotIn("cdn.", html)
        self.assertNotIn("unpkg.com", html)
        for view in ("operations", "regulator", "tenant"):
            self.assertIn(f"data-view=\"{view}\"", html)
        for state in ("loading", "empty", "stale", "error"):
            self.assertIn(state, html)

    def test_refresh_is_single_flight_and_charts_are_reused(self):
        script = (ROOT / "static" / "js" / "app.js").read_text("utf-8")
        self.assertIn("refreshInFlight", script)
        self.assertIn("getInstanceByDom", script)
        self.assertIn("dispose", script)
        self.assertNotIn("alert(", script)

    def test_target_width_has_no_forced_horizontal_overflow(self):
        css = (ROOT / "static" / "css" / "app.css").read_text("utf-8")
        self.assertNotIn("min-width: 1180px", css)
        self.assertIn("@media (max-width: 1400px)", css)


if __name__ == "__main__":
    unittest.main()
