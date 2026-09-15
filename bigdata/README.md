# 电动汽车充电平台第二阶段大数据系统

本目录在第一阶段 Qt、TCP 服务端和 SQLite 数据库基础上，实现项目说明书中的大数据可视化与机器学习智能分析部分。站点和充电桩来自第一阶段已经导入的 UrbanEV 深圳数据，用户、订单、钱包和告警沿用第一阶段业务表结构。

## 已实现内容

1. Hadoop 3.3.0、Spark 3.4.1、Python 3.10 运行环境。
2. 基于第一阶段 SQLite 表生成用户、站点、充电桩、订单、告警和天气模拟数据。
3. 模拟数据中主动加入空值、重复、非法格式、数值越界、外键孤儿、时间倒置、金额不一致和能耗异常值。
4. 使用 PySpark DataFrame 和窗口函数自动发现数据质量问题，输出规则汇总和问题明细。
5. 使用 PySpark 完成去重、类型转换、枚举修正、异常隔离、外键校验、金额重算和天气值修复。
6. 使用 SparkSQL 建立 ODS、DWD、DWS、ADS 四层数据仓库，并同步写入本地 Parquet 和 HDFS。
7. 使用 Flask 提供 ADS 查询接口，Vue 负责页面状态，ECharts 展示经营、设备、质量和预测图表。
8. 使用 Spark MLlib RandomForestRegressor 预测站点未来 1、6、24 小时的充电会话数和空闲桩数，输出 RMSE、MAE、R² 与特征重要性。

## 目录结构

```text
bigdata/
├── pipeline/
│   ├── generate_mock_data.py   # UrbanEV 与第一阶段表结构模拟数据
│   ├── quality_check.py        # PySpark 数据质量发现
│   ├── clean_data.py           # PySpark 清洗与隔离
│   ├── warehouse_etl.py        # SparkSQL 四层数仓
│   └── train_load_model.py     # Spark MLlib 负荷预测
├── dashboard/
│   ├── app.py                  # Flask API
│   ├── templates/index.html    # Vue 页面
│   └── static/                 # ECharts、Vue 与样式
├── scripts/                    # 环境检查、全流程运行和大屏启停
├── sql/warehouse_layers.sql    # 数仓分层口径示例
└── data/                       # 运行后生成，不提交仓库
```

## 数据质量场景

| 类别 | 示例 | 处理方法 |
|---|---|---|
| 完整性 | 手机号、站点名、订单开始时间为空 | 无法修复时进入 quarantine |
| 唯一性 | 用户 ID、手机号、订单号重复 | 窗口函数保留第一条 |
| 有效性 | 手机号格式错误、状态枚举非法 | 隔离或映射到安全默认值 |
| 数值范围 | 负余额、负电量、非法经纬度、零功率 | 修复可恢复值，其余隔离 |
| 参照完整性 | 订单引用不存在的用户或电桩 | 隔离孤儿记录 |
| 业务一致性 | 结束时间早于开始时间、费用不匹配 | 隔离时间错误并重算费用 |
| 异常值 | 单笔电量超过 500kWh | 作为高风险记录隔离 |

## 数仓分层

- ODS：保留 CSV 原始字段、脏数据和接入时间。
- DWD：输出清洗后的用户、站点、电桩、订单、告警和天气明细。
- DWS：形成站点日经营、城市日经营、站点小时负荷和用户价值主题表。
- ADS：形成概览指标、近 30 日营收、站点排名、小时负荷、设备状态、告警、区域贡献和负荷预测。

HDFS 默认根目录为：`/charging_platform`。

## 一次性执行

```bash
cd ~/charging-platform/bigdata
chmod +x scripts/*.sh
./scripts/run_pipeline.sh
./scripts/setup_dashboard.sh
./scripts/start_dashboard.sh
```

Windows 浏览器打开：

```text
http://192.168.142.100:5000
```

## 单步执行

```bash
cd ~/charging-platform/bigdata
source /home/hadoop/.bash_profile

python3 pipeline/generate_mock_data.py
spark-submit --master local[2] pipeline/quality_check.py
spark-submit --master local[2] pipeline/clean_data.py
spark-submit --master local[2] pipeline/warehouse_etl.py
spark-submit --master local[2] pipeline/train_load_model.py
```

## 验收检查

```bash
./scripts/check_environment.sh
hdfs dfs -ls -R /charging_platform | head -100
curl http://127.0.0.1:5000/api/overview
curl http://127.0.0.1:5000/api/model-metrics
```

PyCharm 仅作为可选编辑器，不影响流水线和大屏运行。代码可直接通过 SSH、VS Code Remote 或命令行维护。

## Qt 客户端接入

先启动 Flask 大屏服务，再通过环境变量启动 Qt 服务端：

```bash
export CHARGING_ANALYTICS_URL=http://127.0.0.1:5000
./build/bin/charging_server
```

服务端请求 Flask 的超时为 2 秒。请求成功后按查询条件缓存最后一次结果；Flask 暂时不可用时返回缓存并标记 `stale: true`。没有缓存时仅分析功能返回不可用，找桩、预约、充电、计费和设备控制不受影响。
