"""生成不使用任何训练数据、初始增量严格为零的 Qwen2.5 0.5B LoRA。"""

from __future__ import annotations

import hashlib
import json
import math
import struct
from pathlib import Path

import numpy as np


OUTPUT_DIR = Path(__file__).resolve().parent
OUTPUT_FILE = OUTPUT_DIR / "adapter_model.safetensors"
MANIFEST_FILE = OUTPUT_DIR / "empty_lora_manifest.json"

LAYER_COUNT = 24
HIDDEN_SIZE = 896
KV_SIZE = 128
RANK = 8
SEED = 20260909


def build_tensors() -> dict[str, np.ndarray]:
    """采用 PEFT 默认思路：A 随机初始化、B 全零，因此 BA 恒等于零。"""
    rng = np.random.default_rng(SEED)
    bound = 1.0 / math.sqrt(HIDDEN_SIZE)
    tensors: dict[str, np.ndarray] = {}
    for layer in range(LAYER_COUNT):
        prefix = f"base_model.model.model.layers.{layer}.self_attn"
        for module, output_size in (("q_proj", HIDDEN_SIZE), ("v_proj", KV_SIZE)):
            tensors[f"{prefix}.{module}.lora_A.weight"] = rng.uniform(
                -bound, bound, size=(RANK, HIDDEN_SIZE)
            ).astype("<f4")
            tensors[f"{prefix}.{module}.lora_B.weight"] = np.zeros(
                (output_size, RANK), dtype="<f4"
            )
    return tensors


def save_safetensors(tensors: dict[str, np.ndarray], destination: Path) -> None:
    """写入 safetensors 标准格式，避免生成脚本依赖 PyTorch 或训练框架。"""
    header: dict[str, object] = {
        "__metadata__": {
            "format": "pt",
            "description": "Untrained zero-delta LoRA for Qwen2.5-0.5B-Instruct",
        }
    }
    raw_parts: list[bytes] = []
    offset = 0
    for name in sorted(tensors):
        tensor = np.ascontiguousarray(tensors[name], dtype="<f4")
        raw = tensor.tobytes(order="C")
        header[name] = {
            "dtype": "F32",
            "shape": list(tensor.shape),
            "data_offsets": [offset, offset + len(raw)],
        }
        raw_parts.append(raw)
        offset += len(raw)

    header_bytes = json.dumps(
        header, ensure_ascii=False, separators=(",", ":")
    ).encode("utf-8")
    header_bytes += b" " * ((8 - len(header_bytes) % 8) % 8)
    with destination.open("wb") as stream:
        stream.write(struct.pack("<Q", len(header_bytes)))
        stream.write(header_bytes)
        stream.writelines(raw_parts)


def main() -> None:
    tensors = build_tensors()
    save_safetensors(tensors, OUTPUT_FILE)

    zero_b = all(
        not np.any(value)
        for name, value in tensors.items()
        if name.endswith("lora_B.weight")
    )
    manifest = {
        "baseModel": "Qwen/Qwen2.5-0.5B-Instruct",
        "trained": False,
        "trainingSamples": 0,
        "rank": RANK,
        "alpha": 16,
        "targetModules": ["q_proj", "v_proj"],
        "tensorCount": len(tensors),
        "zeroDeltaVerified": zero_b,
        "seed": SEED,
        "sha256": hashlib.sha256(OUTPUT_FILE.read_bytes()).hexdigest(),
        "sizeBytes": OUTPUT_FILE.stat().st_size,
    }
    MANIFEST_FILE.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
