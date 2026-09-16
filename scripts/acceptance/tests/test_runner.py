import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts.acceptance.runner import checks, load_requirements_map, run_check


class AcceptanceRunnerTest(unittest.TestCase):
    def test_unavailable_external_tool_is_not_run_not_passed(self):
        with tempfile.TemporaryDirectory() as directory:
            report = run_check("hdfs", ["missing-hdfs-binary"], Path(directory))
        self.assertEqual(report["status"], "not_run")

    def test_every_requirement_has_traceability(self):
        matrix = load_requirements_map()
        self.assertFalse([row for row in matrix if not row["implementation"] or not row["test"]])
        self.assertEqual(len({row["id"] for row in matrix}), len(matrix))
        workspace = Path(__file__).resolve().parents[3]
        missing = [row["implementation"] for row in matrix
                   if not (workspace / row["implementation"]).exists()]
        self.assertFalse(missing, f"implementation paths do not exist: {missing}")

    def test_linux_runtime_profile_covers_spark_and_hdfs(self):
        self.assertEqual([name for name, _ in checks("linux-runtime")],
                         ["spark_pipeline", "hdfs"])

    def test_profile_uses_current_python_by_default(self):
        with patch.dict(os.environ, {}, clear=True):
            python_check = checks("local")[0][1]
        self.assertEqual(python_check[0], sys.executable)


if __name__ == "__main__":
    unittest.main()
