"""Deterministic model and feature drift calculations."""

import math


def population_stability_index(baseline_counts: list[int], current_counts: list[int]) -> float:
    if len(baseline_counts) != len(current_counts) or not baseline_counts:
        raise ValueError("matching non-empty bins are required")
    baseline_total = sum(baseline_counts)
    current_total = sum(current_counts)
    if baseline_total <= 0 or current_total <= 0:
        raise ValueError("bin totals must be positive")
    epsilon = 1e-6
    score = 0.0
    for baseline, current in zip(baseline_counts, current_counts):
        expected = max(epsilon, baseline / baseline_total)
        actual = max(epsilon, current / current_total)
        score += (actual - expected) * math.log(actual / expected)
    return score


def drift_level(score: float) -> str:
    if not math.isfinite(score) or score < 0:
        raise ValueError("drift score must be finite and non-negative")
    if score >= 0.25:
        return "severe"
    if score >= 0.10:
        return "attention"
    return "normal"


def build_drift_report(features: dict[str, tuple[list[int], list[int]]],
                       baseline_mae: float, rolling_mae: float) -> dict:
    feature_scores = {name: population_stability_index(*bins)
                      for name, bins in sorted(features.items())}
    error_ratio = rolling_mae / max(baseline_mae, 1e-9)
    score = max([*feature_scores.values(), max(0.0, error_ratio - 1.0)])
    return {"level": drift_level(score), "score": score,
            "featureScores": feature_scores, "errorRatio": error_ratio}
