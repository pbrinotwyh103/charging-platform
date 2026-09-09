# 充电客服合成问答数据

这批数据由项目已有业务功能人工整理后通过固定模板扩充，不是从真实用户对话中抓取的数据，也没有用于当前空 LoRA 的训练。

- 总数据：120 条
- 训练集：108 条
- 验证集：12 条
- 格式：JSONL，每行包含 `messages` 和 `metadata`
- 来源标记：`synthetic-project-aligned`
- 覆盖范围：编号直连、站点查询、电桩状态、预约、充电、结算、钱包、订单、联网、导航和安全处理

重新生成：

```bash
python3 scripts/generate_customer_service_qa.py
```

如果以后使用这批数据训练 LoRA，应另存新的已训练适配器，不要覆盖 `models/qwen2.5-0.5b-empty-lora`，以便保留零训练基线。
