-- 第二阶段数仓口径说明。实际执行代码位于 pipeline/warehouse_etl.py。

-- ODS：原始 CSV 按字符串接入，保留 _row_id 与 ods_ingest_time。
-- DWD：完成类型转换、去重、枚举修复、金额重算和外键孤儿隔离。
-- DWS：按站点日、城市日、站点小时和用户价值主题聚合。
-- ADS：输出大屏概览、营收趋势、站点排名、小时负荷、设备状态、告警和区域指标。

SELECT order_date, station_id, COUNT(*) AS session_count,
       ROUND(SUM(energy_wh) / 1000.0, 3) AS energy_kwh,
       ROUND(SUM(fee_cents) / 100.0, 2) AS revenue_yuan
FROM dwd_charging_orders
WHERE status = 'completed'
GROUP BY order_date, station_id;

