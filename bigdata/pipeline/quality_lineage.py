"""Aggregate data-quality issues into deterministic downstream impact rows."""

from collections import defaultdict


def summarize_lineage(issues: list[dict], downstream_by_entity: dict[str, list[str]]) -> list[dict]:
    grouped: dict[str, dict] = defaultdict(lambda: {"issues": 0, "entities": set(), "outputs": set()})
    for issue in issues:
        rule_id = str(issue.get("ruleId", "")).strip()
        entity_id = str(issue.get("entityId", "")).strip()
        if not rule_id or not entity_id:
            raise ValueError("quality issues require ruleId and entityId")
        row = grouped[rule_id]
        row["issues"] += 1
        row["entities"].add(entity_id)
        row["outputs"].update(downstream_by_entity.get(entity_id, []))
    return [{"ruleId": rule_id, "issueCount": grouped[rule_id]["issues"],
             "entityCount": len(grouped[rule_id]["entities"]),
             "affectedOutputs": sorted(grouped[rule_id]["outputs"])}
            for rule_id in sorted(grouped)]
