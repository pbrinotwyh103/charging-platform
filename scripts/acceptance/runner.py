"""Execute acceptance checks without converting unavailable tools into passes."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAP_PATH = Path(__file__).with_name("requirements_map.json")


def load_requirements_map():
    return json.loads(MAP_PATH.read_text("utf-8"))


def _sanitize(text: str) -> str:
    result = text.replace(str(ROOT), "<workspace>")
    for key in ("CHARGING_API_KEY", "TENCENT_MAP_KEY"):
        value = os.environ.get(key, "")
        if value:
            result = result.replace(value, "<redacted>")
    return result[-12000:]


def run_check(name: str, command: list[str], artifact_dir: Path, *, cwd=ROOT):
    artifact_dir.mkdir(parents=True, exist_ok=True)
    if name == "spark_pipeline" and not shutil.which("spark-submit"):
        return {"name": name, "status": "not_run", "reason": "tool unavailable: spark-submit"}
    if name == "browser" and not (ROOT / "node_modules" / "@playwright" / "test").exists():
        return {"name": name, "status": "not_run", "reason": "dependency unavailable: @playwright/test"}
    executable = command[0]
    if not (Path(executable).is_file() or shutil.which(executable)):
        return {"name": name, "status": "not_run", "reason": f"tool unavailable: {Path(executable).name}"}
    started = datetime.now(timezone.utc)
    try:
        environment = os.environ.copy()
        environment.setdefault("QT_QPA_PLATFORM", "offscreen")
        completed = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, text=True, timeout=600)
        output = _sanitize(completed.stdout + completed.stderr)
        (artifact_dir / f"{name}.log").write_text(output, "utf-8")
        return {"name": name, "status": "passed" if completed.returncode == 0 else "failed",
                "exitCode": completed.returncode,
                "durationMs": int((datetime.now(timezone.utc) - started).total_seconds() * 1000),
                "log": f"{name}.log"}
    except subprocess.TimeoutExpired as error:
        (artifact_dir / f"{name}.log").write_text(_sanitize(str(error)), "utf-8")
        return {"name": name, "status": "failed", "reason": "timeout", "log": f"{name}.log"}


def checks(profile: str):
    python = os.environ.get("CHARGING_ACCEPTANCE_PYTHON", "python3")
    local = [
        ("python", [python, "-m", "unittest", "discover", "-s", "bigdata/tests", "-v"]),
        ("protocol", [str(ROOT / "build/bin/protocol_tests"), "-o", "-,txt"]),
        ("service", [str(ROOT / "build/bin/service_tests"), "-o", "-,txt"]),
        ("business", [str(ROOT / "build/bin/business_integration_tests"), "-o", "-,txt"]),
        ("user_ui", [str(ROOT / "build/bin/user_ui_tests"), "-o", "-,txt"]),
        ("admin_ui", [str(ROOT / "build/bin/admin_ui_tests"), "-o", "-,txt"]),
        ("admin_controller", [str(ROOT / "build/bin/admin_controller_tests"), "-o", "-,txt"]),
    ]
    if profile == "course-vm":
        local += [
            ("spark_pipeline", [str(ROOT / "bigdata/scripts/run_pipeline.sh"), "--local", "--seed", "20260915", "--batch-id", "acceptance-course-vm"]),
            ("hdfs", ["hdfs", "dfs", "-ls", "/charging_platform/ads"]),
            ("browser", ["npx", "playwright", "test", "bigdata/tests/browser/dashboard.spec.js"]),
        ]
    return local
