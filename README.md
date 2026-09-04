# 电动汽车充电桩应用管理平台

## 项目组成

- `charging_user_client`：用户手机客户端。
- `charging_admin_client`：管理员手机客户端。
- `charging_server`：后台TCP、业务和数据库服务。
- `protocol_tests`：公共消息协议自动测试。

当前版本完成项目框架、公共协议编解码、客户端连接封装、服务器连接和消息分发框架、SQLite结构以及协议测试。具体业务功能将在框架确认后按需求矩阵逐项实现。

## 环境

- Ubuntu 22.04
- Qt 6.2.4及以上
- Qt Widgets、Network、SQL、Charts、WebEngineWidgets、Test
- C++17
- SQLite

## 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
  libqt6charts6-dev qt6-webengine-dev libqt6sql6-sqlite
```

## 编译

```bash
cd ~/charging-platform
mkdir -p build
cd build
qmake6 ../charging-platform.pro
make -j$(nproc)
```

也可以直接执行：

```bash
./scripts/build.sh
```

编译结果位于 `build/bin/`。

## 运行协议测试

```bash
cd ~/charging-platform/build
./bin/protocol_tests -v1
```

或在项目根目录执行 `./scripts/run-tests.sh`。

## 启动服务器

```bash
cd ~/charging-platform/build
./bin/charging_server --port 8888 --database data/charging.db
```

## 启动两个手机客户端

在另外两个终端执行：

```bash
cd ~/charging-platform/build
./bin/charging_user_client
```

```bash
cd ~/charging-platform/build
./bin/charging_admin_client
```

同一虚拟机联调时，两个客户端均连接 `127.0.0.1:8888`。
