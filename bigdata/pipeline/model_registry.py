"""Model selection, uncertainty, and atomic model registry utilities."""

from __future__ import annotations

import json
import math
import os
from pathlib import Path


def select_candidate(candidates: list[dict]) -> dict:
    valid = [candidate for candidate in candidates
             if candidate.get("constraintsPassed") is True
             and isinstance(candidate.get("rmse"), (int, float))
             and math.isfinite(candidate["rmse"]) and candidate["rmse"] >= 0]
    if not valid:
        raise ValueError("no valid model candidate")
    return dict(min(valid, key=lambda candidate: (candidate["rmse"], candidate.get("name", ""))))


def prediction_interval(prediction: float, held_out_absolute_residuals: list[float],
                        capacity: int, quantile: float = 0.95) -> dict:
    if capacity < 0 or not held_out_absolute_residuals:
        raise ValueError("capacity and residuals are required")
    ordered = sorted(abs(float(value)) for value in held_out_absolute_residuals)
    index = min(len(ordered) - 1, max(0, math.ceil(quantile * len(ordered)) - 1))
    radius = ordered[index]
    point = min(float(capacity), max(0.0, float(prediction)))
    return {"prediction": point, "lowerBound": max(0.0, point - radius),
            "upperBound": min(float(capacity), point + radius)}


class ModelRegistry:
    def __init__(self, root: Path | str) -> None:
        self.root = Path(root)
        self.versions = self.root / "versions"

    def register(self, metadata: dict) -> Path:
        version = str(metadata.get("modelVersion", "")).strip()
        if not version or "/" in version or "\\" in version:
            raise ValueError("a path-safe modelVersion is required")
        self.versions.mkdir(parents=True, exist_ok=True)
        target = self.versions / f"{version}.json"
        self._atomic_write(target, metadata)
        return target

    def promote(self, version: str) -> dict:
        source = self.versions / f"{version}.json"
        metadata = json.loads(source.read_text("utf-8"))
        promoted = dict(metadata, status="production")
        self._atomic_write(self.root / "current.json", promoted)
        return promoted

    @staticmethod
    def _atomic_write(target: Path, value: dict) -> None:
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_name(f".{target.name}.tmp")
        temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2), "utf-8")
        os.replace(temporary, target)
