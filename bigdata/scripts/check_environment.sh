#!/bin/bash
set +u
source /home/hadoop/.bash_profile

echo "=== 软件版本 ==="
java -version 2>&1 | head -1
hadoop version | head -1
spark-submit --version 2>&1 | grep -m1 'version' || true
python3 --version
python3 -c 'import numpy; print("NumPy", numpy.__version__)'

echo "=== Java 服务进程 ==="
jps -l

echo "=== HDFS ==="
hdfs dfsadmin -report | grep -E 'Safe mode|Live datanodes|Configured Capacity' || true

echo "=== YARN ==="
yarn node -list

echo "=== 关键端口 ==="
ss -lnt | grep -E ':(22|5000|7077|8080|8088|9870)' || true

echo "=== HDFS 数仓目录 ==="
hdfs dfs -ls /charging_platform 2>/dev/null || echo "尚未执行数据流水线"
