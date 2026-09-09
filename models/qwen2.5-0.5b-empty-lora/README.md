# Qwen2.5 0.5B 空 LoRA

这是为 `Qwen/Qwen2.5-0.5B-Instruct` 准备的标准 PEFT LoRA 初始适配器，没有读取训练集，也没有执行训练。

- rank：8
- alpha：16
- 目标层：每个 Transformer 层的 `q_proj`、`v_proj`
- A 矩阵：固定随机种子进行标准初始化
- B 矩阵：全部为零
- 初始效果：`B × A = 0`，所以加载 LoRA 不会改变基础模型输出

目录中的 `adapter_model.safetensors` 可以和 `adapter_config.json` 一起由 PEFT 加载：

```python
from peft import PeftModel
from transformers import AutoModelForCausalLM

base = AutoModelForCausalLM.from_pretrained("Qwen/Qwen2.5-0.5B-Instruct")
model = PeftModel.from_pretrained(base, "models/qwen2.5-0.5b-empty-lora")
```

如需重新生成，使用包含 NumPy 的 Python 环境运行：

```bash
python models/qwen2.5-0.5b-empty-lora/generate_empty_lora.py
```

`empty_lora_manifest.json` 会记录未训练状态、张量数量、零增量检查结果和文件 SHA-256。
