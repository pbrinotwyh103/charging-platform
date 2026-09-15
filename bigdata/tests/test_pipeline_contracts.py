import json
import tempfile
import unittest
from pathlib import Path

from bigdata.pipeline.quality_lineage import summarize_lineage
from bigdata.pipeline.run_manifest import RunManifest


class PipelineContractTest(unittest.TestCase):
    def test_manifest_is_versioned_and_published_atomically(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "orders.csv"
            source.write_text("order_id\n1\n", encoding="utf-8")
            run = RunManifest(root / "warehouse", batch_id="batch-1",
                              clock=lambda: "2026-09-15T10:00:00Z")
            run.start([source])
            run.publish("ods", [source], row_count=1)
            manifest = run.complete()

            self.assertEqual(manifest["batchId"], "batch-1")
            self.assertEqual(manifest["status"], "completed")
            self.assertEqual(len(manifest["inputs"][0]["sha256"]), 64)
            current = json.loads((root / "warehouse" / "current.json").read_text("utf-8"))
            self.assertEqual(current, manifest)
            self.assertFalse((root / "warehouse" / ".current.json.tmp").exists())

    def test_failed_run_does_not_replace_last_successful_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "warehouse"
            first = RunManifest(root, "ok", clock=lambda: "2026-09-15T10:00:00Z")
            first.start([])
            first.complete()
            failed = RunManifest(root, "bad", clock=lambda: "2026-09-15T11:00:00Z")
            failed.start([])
            failed.fail("dwd", "broken")
            current = json.loads((root / "current.json").read_text("utf-8"))
            self.assertEqual(current["batchId"], "ok")
            failure = json.loads((root / "runs" / "bad" / "manifest.json").read_text("utf-8"))
            self.assertEqual(failure["failure"]["stage"], "dwd")

    def test_quality_lineage_reports_unique_sorted_downstream_impact(self):
        report = summarize_lineage(
            [{"ruleId": "ORDER_TIME", "entityId": "o1"},
             {"ruleId": "ORDER_TIME", "entityId": "o1"},
             {"ruleId": "PHONE", "entityId": "u1"}],
            {"o1": ["dwd_orders", "ads_overview"], "u1": ["dwd_users"]})
        self.assertEqual(report, [
            {"ruleId": "ORDER_TIME", "issueCount": 2, "entityCount": 1,
             "affectedOutputs": ["ads_overview", "dwd_orders"]},
            {"ruleId": "PHONE", "issueCount": 1, "entityCount": 1,
             "affectedOutputs": ["dwd_users"]},
        ])


if __name__ == "__main__":
    unittest.main()
