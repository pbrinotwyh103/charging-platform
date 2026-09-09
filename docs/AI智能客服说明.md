# AI 智能客服

用户端的“客服”页面通过充电平台 TCP 协议向服务端发送问题，服务端再调用本机的 Qwen 推理服务。用户端不直接访问模型，适合局域网多客户端演示。

默认使用 `llama.cpp` 加载官方 `Qwen2.5-0.5B-Instruct` 的 Q4_K_M 量化文件，模型约 491 MB。相比完整 Ollama 运行时，这种方式更轻量。

## 启动

```bash
llama serve -hf Qwen/Qwen2.5-0.5B-Instruct-GGUF:Q4_K_M \
  --host 127.0.0.1 --port 8080 -c 1024
```

课程虚拟机已经下载模型时，可直接使用项目脚本：

```bash
bash scripts/start-qwen-service.sh
```

保持 Qwen 服务运行后，正常启动 `charging_server`。客户端登录后点击底部“客服”即可提问。

可通过环境变量替换模型或接口：

```bash
export QWEN_MODEL=qwen2.5-0.5b-instruct
export QWEN_CHAT_URL=http://127.0.0.1:8080/v1/chat/completions
```

接口也兼容 Ollama 的 `/api/chat` 格式，可继续使用旧的 `OLLAMA_MODEL` 和 `OLLAMA_CHAT_URL` 环境变量。如果模型服务暂时不可用，服务端会自动使用内置充电知识库回复，客户端会明确显示“模型未启动”。

## 空 LoRA

`models/qwen2.5-0.5b-empty-lora` 保存了一个未经训练的标准 PEFT LoRA。它没有读取任何训练数据，所有 B 矩阵均为零，因此初始增量严格为零，不会改变基础模型的回答。该目录主要用于后续课程训练或作为“尚未训练”的适配器基线；当前客服默认仍直接运行基础 GGUF 模型。
