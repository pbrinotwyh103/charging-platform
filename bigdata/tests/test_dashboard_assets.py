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

    def test_dashboard_uses_persisted_design_tokens(self):
        css = (ROOT / "static" / "css" / "app.css").read_text("utf-8")
        for token in (
            "--color-bg-canvas",
            "--color-bg-surface",
            "--color-text-primary",
            "--color-primary",
            "--space-1",
            "--radius-panel",
            "--control-height",
            "--duration-fast",
        ):
            self.assertIn(token, css)
        self.assertIn("font-variant-numeric: tabular-nums", css)
        self.assertIn("prefers-reduced-motion: reduce", css)

    def test_dashboard_uses_iot_dark_palette(self):
        css = (ROOT / "static" / "css" / "app.css").read_text("utf-8").lower()
        self.assertIn("--color-bg-canvas: #0f172a", css)
        self.assertIn("--color-bg-surface: #1b2336", css)
        self.assertIn("--color-primary: #22c55e", css)
        self.assertIn("--color-border: #475569", css)

        script = (ROOT / "static" / "js" / "app.js").read_text("utf-8").lower()
        self.assertIn("green: '#22c55e'", script)
        self.assertIn("surface: '#1b2336'", script)

    def test_shared_shell_has_accessible_navigation_and_states(self):
        html = (ROOT / "templates" / "index.html").read_text("utf-8")
        self.assertIn('class="skip-link"', html)
        self.assertIn('id="main-content"', html)
        self.assertIn('role="tablist"', html)
        self.assertIn(':aria-selected=', html)
        self.assertIn('aria-live="polite"', html)
        self.assertIn('class="page-container"', html)
        self.assertIn('@keydown="handleViewTabKeydown', html)

        script = (ROOT / "static" / "js" / "app.js").read_text("utf-8")
        self.assertIn("handleViewTabKeydown", script)
        self.assertIn("ArrowRight", script)
        self.assertIn("ArrowLeft", script)

    def test_chart_theme_uses_design_system_palette(self):
        script = (ROOT / "static" / "js" / "app.js").read_text("utf-8")
        self.assertIn("fontFamily", script)
        self.assertIn("axisPointer", script)
        self.assertIn("animationDuration", script)
        self.assertIn("prefers-reduced-motion", script)

    def test_operations_view_uses_analytics_hierarchy_and_accessible_charts(self):
        html = (ROOT / "templates" / "index.html").read_text("utf-8")
        self.assertIn('class="view-heading"', html)
        self.assertIn('class="metric-strip"', html)
        self.assertIn('class="chart-summary"', html)
        self.assertIn('<caption>', html)
        self.assertIn('aria-sort=', html)

        script = (ROOT / "static" / "js" / "app.js").read_text("utf-8")
        self.assertNotIn("roseType", script)
        self.assertNotIn("echarts.graphic.LinearGradient", script)
        self.assertIn("lowerBound", script)
        self.assertIn("upperBound", script)

    def test_regulator_and_tenant_views_are_structured_not_raw_json_only(self):
        html = (ROOT / "templates" / "index.html").read_text("utf-8")
        self.assertIn('class="regulator-summary metric-strip"', html)
        self.assertIn('model-comparison-table', html)
        self.assertIn('drift-summary', html)
        self.assertIn('<advice-table', html)
        self.assertIn('<details class="raw-data"', html)

        script = (ROOT / "static" / "js" / "app.js").read_text("utf-8")
        self.assertIn("AdviceTable", script)
        self.assertIn("severityLabel", script)


if __name__ == "__main__":
    unittest.main()
