"""Privacy-safe structured analytics access audit."""

import json
import os
from datetime import datetime, timezone
from pathlib import Path


def record(event: dict) -> None:
    path = os.environ.get("CHARGING_ANALYTICS_AUDIT")
    if not path:
        return
    safe = {key: value for key, value in event.items()
            if key not in {"password", "apiKey", "phone", "payload"}}
    safe["timestamp"] = datetime.now(timezone.utc).isoformat()
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(safe, ensure_ascii=False) + "\n")
