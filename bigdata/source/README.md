# 第一阶段数据兼容快照

本目录由第一阶段 `database/charging.db` 导出，只包含第二阶段生成模拟数据所需的字段。

- `phase1_users.csv`：第一阶段用户基础数据。
- `urbanev_stations.csv`：第一阶段站点表与 UrbanEV 原始站点记录的关联结果。
- `urbanev_piles.csv`：第一阶段充电桩表与 UrbanEV 原始电桩记录的关联结果。

提供 CSV 快照是因为课程大数据镜像内置的 SQLite 版本较旧，无法解析第一阶段数据库使用的部分索引语法。快照不改变原数据库，也不改变业务表结构。
