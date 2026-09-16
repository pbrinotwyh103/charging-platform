#!/usr/bin/env python3
import json
import shutil
import sys

tools = {name: shutil.which(name) for name in ("java", "hadoop", "hdfs", "spark-submit", "python3")}
print(json.dumps(tools, ensure_ascii=False, indent=2))
raise SystemExit(0 if tools["python3"] else 1)
