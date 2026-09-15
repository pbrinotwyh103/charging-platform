#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

from runner import ROOT, checks, load_requirements_map, run_check


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=("local", "course-vm"), default="local")
    parser.add_argument("--verify-traceability", action="store_true")
    parser.add_argument("--verify-all-passed", action="store_true")
    args = parser.parse_args()
    requirements = load_requirements_map()
    incomplete = [row["id"] for row in requirements if not row.get("implementation") or not row.get("test")]
    if args.verify_traceability:
        if incomplete:
            print("缺少追踪信息：" + ", ".join(incomplete))
            return 1
        print(f"需求追踪完整：{len(requirements)} 项")
        return 0
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"-{args.profile}"
    output = ROOT / "artifacts" / "acceptance" / run_id
    results = [run_check(name, command, output) for name, command in checks(args.profile)]
    manifest = {"runId": run_id, "profile": args.profile,
                "generatedAt": datetime.now(timezone.utc).isoformat(), "checks": results,
                "summary": {state: sum(row["status"] == state for row in results)
                            for state in ("passed", "failed", "not_run")}}
    output.mkdir(parents=True, exist_ok=True)
    (output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), "utf-8")
    print(json.dumps(manifest["summary"], ensure_ascii=False))
    if args.verify_all_passed and any(row["status"] != "passed" for row in results):
        return 1
    return 1 if any(row["status"] == "failed" for row in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())
