#!/usr/bin/env python3
"""Run the dashboard browser checks with a managed local Flask process."""

from __future__ import annotations

import os
import socket
import subprocess
import sys
import time
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def available_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def wait_until_ready(url: str, process: subprocess.Popen, timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("dashboard process exited before becoming ready")
        try:
            with urllib.request.urlopen(f"{url}/api/health", timeout=1) as response:
                if response.status == 200:
                    return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("dashboard did not become ready before timeout")


def main() -> int:
    port = available_port()
    url = f"http://127.0.0.1:{port}"
    environment = os.environ.copy()
    environment["DASHBOARD_PORT"] = str(port)
    dashboard = subprocess.Popen(
        [sys.executable, "-m", "bigdata.dashboard.app"],
        cwd=ROOT,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        wait_until_ready(url, dashboard)
        environment["DASHBOARD_URL"] = url
        result = subprocess.run(
            ["npx", "playwright", "test", "bigdata/tests/browser/dashboard.spec.js"],
            cwd=ROOT,
            env=environment,
            check=False,
        )
        return result.returncode
    finally:
        dashboard.terminate()
        try:
            dashboard.wait(timeout=5)
        except subprocess.TimeoutExpired:
            dashboard.kill()
            dashboard.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
