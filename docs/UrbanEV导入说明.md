# UrbanEV 静态站点数据

来源：[论文](https://www.nature.com/articles/s41597-025-04874-4)、[作者仓库](https://github.com/IntelligentSystemsLab/UrbanEV)。完整压缩包已通过作者公开 Google Drive 镜像下载到本机项目上一级 `datasets/urbanev/UrbanEVDataset.zip`（647305188 字节）。许可为 CC0-1.0。

## 本次结果

2026-09-08 已导入虚拟机数据库 `/home/bit/charging-platform/build-fresh/data/charging.db`，并将一致性快照提交为仓库中的 `database/charging.db`：新增 1682 个站点、22568 个有效电桩。另有 82 条功率无效的电桩完整保留在 `urbanev_piles`，其 `pile_id` 为 NULL，原因见 `rejection_reason`。原有 3 个站点、3 个电桩、2 个用户均保留。

导入前备份：`/home/bit/charging-platform/build-fresh/data/charging.db.before-urbanev-20260908-085331-846226.bak`。

虚拟机 CSV 和执行报告位于 `/home/bit/datasets/urbanev/`。本机报告位于项目上一级 `datasets/urbanev/import-report.json`。

## 数据含义与限制

- 经纬度对应充电站，不是每个桩独立测量的位置。同坐标不同来源 ID 保留，不擅自合并。
- 作者最新 README 标注 GCJ-02，旧 Dryad 文档标注 WGS84；保留原始数值、不额外转换，冲突记录在来源信息中。
- 时间范围为 2022-09-01 至 2023-02-28，不代表当前实际运营情况。未导入历史曲线、收费流水或用户个人数据。
- 未提供真实站名、街道地址和现行价格，因此采用 `深圳-<来源ID>` 标签，地址注明资料缺失。价格统一设为 **1.50 元/千瓦时**，仅供课程演示，不是数据集提供或核实过的真实电价。
- 站点设为 online 供位置查询。为课堂业务演示，电桩使用来源编号确定性生成约 70% idle、15% charging、10% offline、5% fault 的模拟状态；这些不是数据集提供的实时状态。AC 映射 slow，DC 映射 fast。
- `urbanev_stations`、`urbanev_piles` 保存来源 ID、业务表 ID、原始记录；`urbanev_import_runs` 保存来源、CSV SHA-256、导入结果。重复导入跳过既有来源 ID，不覆盖后续管理员修改。

## 运行与再次导入

最新版服务端必须明确指定上述数据库；不要从另一目录启动后误用新建的空库：

```bash
/home/bit/charging-platform-release-81d2716/build/bin/charging_server --port 8888 --database /home/bit/charging-platform/build-fresh/data/charging.db
```

从 GitHub 新克隆项目后，也可以在项目根目录直接使用仓库快照：

```bash
./build/bin/charging_server --port 8888 --database database/charging.db
```

再次导入命令（通常不需要重复执行）：

```bash
python3 /home/bit/charging-platform-release-81d2716/scripts/import_urbanev.py \
  --db /home/bit/charging-platform/build-fresh/data/charging.db \
  --data-dir /home/bit/datasets/urbanev \
  --report /home/bit/datasets/urbanev/import-report-repeat.json
```

客户端模拟定位已设为深圳市中心（纬度 22.5431、经度 114.0579），附近搜索半径为 10 公里。例如来源站 1001 的经度为 113.784724、纬度为 22.714121。

已在数据库快照验证首次导入及重复导入（重复新增为 0），实际导入事务通过 SQLite 完整性和外键检查。此次没有进行全工程验证，也没有推送 GitHub。
