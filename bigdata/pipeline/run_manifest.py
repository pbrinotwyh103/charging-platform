"""Versioned, atomic publication metadata for analytics pipeline runs."""

from __future__ import annotations

import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Iterable


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class RunManifest:
    def __init__(self, root: Path | str, batch_id: str,
                 clock: Callable[[], str] = _utc_now,
                 schema_version: str = "2") -> None:
        normalized = batch_id.strip()
        if not normalized or "/" in normalized or "\\" in normalized:
            raise ValueError("batch_id must be a non-empty path-safe value")
        self.root = Path(root)
        self.batch_id = normalized
        self.clock = clock
        self.run_dir = self.root / "runs" / normalized
        self.manifest = {
            "batchId": normalized,
            "dataVersion": normalized,
            "schemaVersion": schema_version,
            "status": "created",
            "inputs": [],
            "layers": {},
        }

    def start(self, input_paths: Iterable[Path | str]) -> dict:
        inputs = []
        for value in sorted((Path(path) for path in input_paths), key=lambda path: str(path)):
            if not value.is_file():
                raise FileNotFoundError(value)
            inputs.append({"name": value.name, "sizeBytes": value.stat().st_size,
                           "sha256": _sha256(value)})
        self.manifest.update({"status": "running", "startedAt": self.clock(), "inputs": inputs})
        self._write_run_manifest()
        return dict(self.manifest)

    def publish(self, layer: str, outputs: Iterable[Path | str], row_count: int) -> dict:
        if self.manifest["status"] != "running":
            raise RuntimeError("run must be active before publishing")
        if row_count < 0:
            raise ValueError("row_count cannot be negative")
        files = [Path(path) for path in outputs]
        self.manifest["layers"][layer] = {
            "rowCount": row_count,
            "outputs": [{"name": path.name, "sha256": _sha256(path)} for path in files],
            "publishedAt": self.clock(),
        }
        self._write_run_manifest()
        return dict(self.manifest["layers"][layer])

    def complete(self) -> dict:
        self.manifest.update({"status": "completed", "completedAt": self.clock()})
        self._write_run_manifest()
        self.root.mkdir(parents=True, exist_ok=True)
        temporary = self.root / ".current.json.tmp"
        temporary.write_text(json.dumps(self.manifest, ensure_ascii=False, indent=2), "utf-8")
        os.replace(temporary, self.root / "current.json")
        return dict(self.manifest)

    def fail(self, stage: str, message: str) -> dict:
        self.manifest.update({"status": "failed", "failedAt": self.clock(),
                              "failure": {"stage": stage, "message": message}})
        self._write_run_manifest()
        return dict(self.manifest)

    def _write_run_manifest(self) -> None:
        self.run_dir.mkdir(parents=True, exist_ok=True)
        target = self.run_dir / "manifest.json"
        temporary = self.run_dir / ".manifest.json.tmp"
        temporary.write_text(json.dumps(self.manifest, ensure_ascii=False, indent=2), "utf-8")
        os.replace(temporary, target)
