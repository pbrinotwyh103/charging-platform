import json
import math
import tempfile
import unittest
from pathlib import Path

from bigdata.pipeline.incremental_features import publish_checkpoint
from bigdata.pipeline.model_drift import drift_level, population_stability_index
from bigdata.pipeline.model_registry import ModelRegistry, prediction_interval, select_candidate


class ModelLifecycleTest(unittest.TestCase):
    def test_selects_lowest_rmse_candidate_that_passes_constraints(self):
        selected = select_candidate([
            {"name": "linear_regression", "rmse": 5.0, "constraintsPassed": True},
            {"name": "random_forest", "rmse": 3.0, "constraintsPassed": True},
            {"name": "gradient_boosted_trees", "rmse": 1.0, "constraintsPassed": False},
        ])
        self.assertEqual(selected["name"], "random_forest")

    def test_invalid_candidates_do_not_replace_production_model(self):
        with self.assertRaises(ValueError):
            select_candidate([{"name": "bad", "rmse": math.nan,
                               "constraintsPassed": True}])

    def test_prediction_interval_uses_held_out_residual_and_capacity_bounds(self):
        self.assertEqual(prediction_interval(8.0, [1.0, 2.0, 4.0, 3.0], 10),
                         {"prediction": 8.0, "lowerBound": 4.0, "upperBound": 10.0})

    def test_registry_promotes_atomically_and_retains_history(self):
        with tempfile.TemporaryDirectory() as directory:
            registry = ModelRegistry(Path(directory))
            registry.register({"modelVersion": "m1", "rmse": 3.2})
            registry.promote("m1")
            current = json.loads((Path(directory) / "current.json").read_text("utf-8"))
            self.assertEqual(current["modelVersion"], "m1")
            self.assertTrue((Path(directory) / "versions" / "m1.json").exists())

    def test_drift_levels_and_distribution_distance_are_stable(self):
        self.assertEqual(drift_level(0.08), "normal")
        self.assertEqual(drift_level(0.18), "attention")
        self.assertEqual(drift_level(0.31), "severe")
        self.assertAlmostEqual(population_stability_index([50, 50], [50, 50]), 0.0)
        self.assertGreater(population_stability_index([90, 10], [10, 90]), 1.0)

    def test_checkpoint_advances_only_after_outputs_exist(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "features.parquet"
            with self.assertRaises(FileNotFoundError):
                publish_checkpoint(root / "checkpoint.json", [output], "2026-09-15T12:00:00Z")
            output.write_text("ready", "utf-8")
            checkpoint = publish_checkpoint(root / "checkpoint.json", [output], "2026-09-15T12:00:00Z")
            self.assertEqual(checkpoint["watermark"], "2026-09-15T12:00:00Z")


if __name__ == "__main__":
    unittest.main()
