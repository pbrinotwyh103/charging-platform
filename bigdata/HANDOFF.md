# 第二阶段大数据项目交接说明

## 一 当前交付状态

交接分支：`feat/stage2-bigdata`

第二阶段主链路已经在课程大数据虚拟机中完整运行成功：

```text
第一阶段 SQLite 与 UrbanEV
        ↓
模拟脏数据
        ↓
PySpark 数据质量检测
        ↓
PySpark 数据清洗与异常隔离
        ↓
SparkSQL ODS → DWD → DWS → ADS
        ↓
Spark MLlib 负荷预测
        ↓
Flask API → Vue → ECharts 大屏
```

当前虚拟机地址为 `192.168.142.100`，账号和密码均为 `hadoop`。项目部署目录为：

```text
/home/hadoop/charging-platform/bigdata
```

大屏当前可通过以下地址访问：

```text
http://192.168.142.100:5000
```

## 二 对照任务要求的完成情况

| 序号 | 要求 | 状态 | 实现位置与说明 |
|---|---|---|---|
| 1 | 安装 Hadoop | 已完成 | 虚拟机内 Hadoop 3.3.0，HDFS、YARN 均能启动，HDFS 可正常读写 |
| 2 | 安装 PyCharm | 未安装，非阻塞 | 用户确认可以不安装；现有代码可通过 SSH、命令行或其他编辑器维护 |
| 3 | 基于第一阶段表结构生成带质量问题的模拟数据 | 已完成 | `pipeline/generate_mock_data.py`，使用第一阶段用户表以及 UrbanEV 站点、电桩数据，默认生成 10 万条订单 |
| 4 | PySpark 发现数据质量问题 | 已完成 | `pipeline/quality_check.py`，覆盖完整性、唯一性、有效性、参照完整性、一致性和异常值规则 |
| 5 | PySpark 数据清洗 | 已完成 | `pipeline/clean_data.py`，完成去重、类型转换、默认值修复、费用重算、外键校验和隔离区输出 |
| 6 | SparkSQL 完成 ODS、DWD、DWS、ADS | 已完成 | `pipeline/warehouse_etl.py`，本地和 HDFS 同步输出 Parquet 分层表 |
| 7 | Flask、Vue、ECharts 可视化 | 已完成 | `dashboard/`，Vue 和 ECharts 已放入 `static/vendor`，不依赖 CDN；Flask 接口已验证 |
| 8 | Spark MLlib 数据预测 | 已完成基础版本 | `pipeline/train_load_model.py`，随机森林预测未来 1、6、24 小时站点会话数与空闲桩数 |

## 三 已完成的代码与数据设计

### 1 模拟数据

默认数据规模：

- 用户：1001 行，其中包含重复和异常场景。
- UrbanEV 站点：301 行原始记录。
- UrbanEV 充电桩：3000 行原始记录。
- 充电订单：100001 行原始记录。
- 告警：1250 行原始记录。
- 天气：93 行原始记录。

主动加入的数据质量问题包括：

- 必填字段为空。
- 用户 ID、手机号、站点 ID、订单号等业务键重复。
- 手机号、时间字段格式错误。
- 经度、纬度、价格、余额、功率、电量和费用越界。
- 订单引用不存在的用户、站点或充电桩。
- 告警引用不存在的充电桩。
- 订单时间倒置、时长为负。
- 费用与电量乘单价不一致。
- 单笔充电量极端异常。

### 2 第一阶段与 UrbanEV 数据来源

课程虚拟机自带 SQLite 版本过旧，不能解析第一阶段数据库使用的部分索引语法，因此没有修改原数据库，而是导出了兼容 CSV 快照：

```text
bigdata/source/phase1_users.csv
bigdata/source/urbanev_stations.csv
bigdata/source/urbanev_piles.csv
```

生成器优先读取现代 SQLite；遇到旧 SQLite 兼容问题时自动切换到上述快照。站点和电桩均保留 `urbanev_source_id`、TAZ 或桩类型来源字段。

### 3 数据质量检测

质量检测使用 PySpark DataFrame、条件表达式、窗口函数和关联检查实现，输出：

```text
bigdata/data/quality/quality_report.json
bigdata/data/quality/quality_summary.json
bigdata/data/quality/issues/parquet/
bigdata/data/quality/issues/csv/
```

最近一次运行扫描 `105646` 行，检测出 `46` 个显式规则问题，问题率为 `0.0435%`。

### 4 数据清洗

清洗后的有效数据：

| 数据表 | 原始行数 | 清洗后行数 | 去重或隔离数量 |
|---|---:|---:|---:|
| users | 1001 | 998 | 3 |
| stations | 301 | 296 | 5 |
| charging_piles | 3000 | 2889 | 111 |
| charging_orders | 100001 | 96588 | 3413 |
| alarms | 1250 | 1202 | 48 |
| weather_daily | 93 | 92 | 1 |

隔离区共写入 `3575` 条明细。清洗结果和隔离结果同时输出 CSV 与 Parquet。

### 5 数仓分层

- ODS：按字符串保留原始 CSV、`_row_id` 和接入时间。
- DWD：保存清洗后的用户、站点、电桩、订单、告警、天气明细。
- DWS：站点日经营、城市日经营、站点小时负荷、用户价值主题。
- ADS：概览指标、近 30 日营收、站点排名、小时负荷、桩状态、告警分布、区域指标和预测结果。

HDFS 根目录：

```text
/charging_platform/ods
/charging_platform/dwd
/charging_platform/dws
/charging_platform/ads
/charging_platform/models
```

### 6 MLlib 模型

当前使用 `RandomForestRegressor`，特征包括站点、小时、星期、周末、节假日、温度、降雨、价格、电桩数、快充桩数和不可用桩数。

最近一次运行结果：

- 有效订单：96588。
- 预测记录：60 条，即 20 个重点站点乘 1、6、24 小时三个预测时段。
- RMSE：0.6613。
- R²：0.2207。

模型及指标输出：

```text
bigdata/data/models/station_load_rf/
bigdata/data/exports/ads/model_metrics.json
bigdata/data/exports/ads/predictions.json
```

### 7 可视化大屏

Flask 已提供以下接口：

```text
/api/health
/api/overview
/api/revenue-trend
/api/station-rank
/api/hourly-load
/api/pile-status
/api/alarm-distribution
/api/district-metrics
/api/quality
/api/predictions
/api/model-metrics
```

大屏包含累计营收、充电电量、订单量、站点电桩数、数据质量、模型指标、营收趋势、设备状态、小时负荷、站点排名、区域贡献和未来负荷预测。

## 四 已完成的验证

1. 全部 Python 文件通过 `compileall` 语法检查。
2. 模拟数据生成器在 Windows 小规模数据集上通过测试。
3. 完整流水线在大数据虚拟机上返回成功。
4. HDFS 四层目录和模型目录已成功写入。
5. Flask `/api/health` 返回 `ok`。
6. Flask 概览接口返回 296 个有效站点、2889 个有效电桩、96588 条有效订单、总营收 3550792.20 元。
7. 模型接口返回 RMSE 0.6613、R² 0.2207。

## 五 尚未完成和建议继续处理的内容

### 必须继续检查

1. 在浏览器中逐个检查大屏图表尺寸、中文字体、窗口缩放和全屏展示效果。目前只验证了 HTTP 页面与接口，尚未进行正式视觉验收。
2. 为 `quality_check.py`、`clean_data.py`、`warehouse_etl.py` 增加自动化单元测试。目前主要依赖完整流水线集成验证。
3. 整理第二阶段最终成果物目录，包括源代码、运行说明、测试用例、数据质量报告、数仓说明和演示录屏。

### 建议优化

1. 当前模拟订单随机性较强，模型 R² 为 0.2207。可在生成器中增强早晚高峰、工作日、降雨、节假日和站点差异对订单量的影响，再重新训练，提高可解释性和演示效果。
2. 当前 Flask 使用内置开发服务器，课堂演示足够；若长期部署，可将启动脚本改为 Gunicorn。
3. 当前预测结果只展示在 Web 大屏，尚未回写第一阶段 Qt 用户端的“低拥堵站点推荐”和管理员端的“负荷预警”。如项目最终验收要求端到端联动，需要新增 Qt 服务端接口。
4. 数据质量报告检测的是原始规则问题；清洗阶段还会因为无效维度被移除而进一步隔离关联订单。后续可以增加“清洗后级联影响”专题统计，让 46 个显式问题与 3575 条隔离记录的关系更直观。
5. PyCharm 尚未安装。如团队习惯使用 PyCharm，可将虚拟机目录映射或配置 SSH 远程解释器，但不影响现有运行。

## 六 接手后的推荐顺序

1. 拉取 `feat/stage2-bigdata` 分支并阅读本文件和 `README.md`。
2. 启动大数据虚拟机，确认地址仍为 `192.168.142.100`。
3. 运行环境检查脚本。
4. 重新运行完整数据流水线。
5. 启动大屏并进行浏览器视觉检查。
6. 优化模型数据规律和图表细节。
7. 补充测试用例、成果物文档和演示材料。

## 七 常用命令

```bash
ssh hadoop@192.168.142.100
cd ~/charging-platform/bigdata

# 环境与服务检查
./scripts/check_environment.sh

# 完整重跑
./scripts/run_pipeline.sh

# 首次准备 Flask、Vue 和 ECharts
./scripts/setup_dashboard.sh

# 启动大屏
./scripts/start_dashboard.sh

# 停止大屏
./scripts/stop_dashboard.sh

# 查看 HDFS 分层结果
hdfs dfs -ls -R /charging_platform | head -100
```

Windows 浏览器访问：

```text
http://192.168.142.100:5000
```

## 八 注意事项

- 不要把 `bigdata/data/` 和 `bigdata/logs/` 提交到 Git，这些目录会由流水线重新生成。
- Vue 和 ECharts 已放入源码目录，可以离线运行。
- 不要重新格式化 NameNode；正常情况下只需启动 Hadoop 并退出安全模式。
- 原第一阶段 Qt 工程没有被第二阶段代码修改。
- 如果虚拟机 IP 变化，应同步修改访问地址，但 Hadoop 主机名 `node100` 必须能够解析到虚拟机实际 IP。
