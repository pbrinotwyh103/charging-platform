#!/usr/bin/env bash
set -euo pipefail

# 课堂演示默认使用最小的 Qwen2.5 0.5B 量化模型，也可用环境变量覆盖。
qwen_model_file="${QWEN_MODEL_FILE:-/home/bit/models/qwen2.5-0.5b-instruct-q4_k_m.gguf}"
qwen_runner="${LLAMA_BIN:-/home/bit/.local/bin/llama}"
qwen_log="${QWEN_LOG_FILE:-/home/bit/qwen-service.log}"

if curl -fsS --max-time 2 http://127.0.0.1:8080/health >/dev/null 2>&1; then
    echo "Qwen 客服服务已经运行：127.0.0.1:8080"
    exit 0
fi

if [[ ! -x "$qwen_runner" ]]; then
    echo "未找到 llama.cpp：$qwen_runner" >&2
    exit 1
fi
if [[ ! -f "$qwen_model_file" ]]; then
    echo "未找到 Qwen 模型：$qwen_model_file" >&2
    exit 1
fi

nohup "$qwen_runner" serve -m "$qwen_model_file" \
    --host 127.0.0.1 --port 8080 -c 1024 -t 2 \
    >"$qwen_log" 2>&1 &

for _ in $(seq 1 30); do
    if curl -fsS --max-time 2 http://127.0.0.1:8080/health >/dev/null 2>&1; then
        echo "Qwen 客服服务启动成功：127.0.0.1:8080"
        exit 0
    fi
    sleep 1
done

echo "Qwen 客服服务启动超时，请查看日志：$qwen_log" >&2
exit 1
