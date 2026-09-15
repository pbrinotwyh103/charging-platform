import json
import tempfile
import unittest
import os
import subprocess
import sys
from pathlib import Path

from bigdata.pipeline.quality_lineage import summarize_lineage
from bigdata.pipeline.run_manifest import RunManifest
from bigdata.pipeline.run_manifest import current_run_metadata


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

    def test_current_run_metadata_requires_no_external_runtime(self):
        metadata = current_run_metadata(
            {"CHARGING_BATCH_ID": "batch-7", "CHARGING_DATA_VERSION": "data-3"},
            clock=lambda: "2026-09-15T12:00:00Z")
        self.assertEqual(metadata, {"batch_id": "batch-7", "data_version": "data-3",
                                    "generated_at": "2026-09-15T12:00:00Z"})

    def test_mock_generator_honors_small_configured_sizes(self):
        with tempfile.TemporaryDirectory() as directory:
            environment = dict(os.environ, CHARGING_BIGDATA_DATA=directory)
            result = subprocess.run([
                sys.executable, "bigdata/pipeline/generate_mock_data.py",
                "--source-db", f"{directory}/missing.db", "--users", "40",
                "--stations", "10", "--piles", "20", "--orders", "50",
                "--days", "10", "--seed", "9", "--batch-id", "small"],
                cwd=Path(__file__).resolve().parents[2], env=environment,
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest = json.loads((Path(directory) / "ods_raw" / "manifest.json").read_text("utf-8"))
            self.assertEqual(manifest["batch_id"], "small")


if __name__ == "__main__":
    unittest.main()
