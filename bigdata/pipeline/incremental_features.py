"""Transactional checkpoint publication for incremental feature jobs."""

import json
import os
from pathlib import Path


def publish_checkpoint(path: Path | str, outputs: list[Path | str], watermark: str) -> dict:
    output_paths = [Path(output) for output in outputs]
    missing = [output for output in output_paths if not output.exists()]
    if missing:
        raise FileNotFoundError(missing[0])
    target = Path(path)
    value = {"watermark": watermark, "outputs": [output.name for output in output_paths]}
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(f".{target.name}.tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2), "utf-8")
    os.replace(temporary, target)
    return value
