# 电动汽车充电桩应用管理平台

## 项目组成

- `charging_user_client`：用户手机客户端。
- `charging_admin_client`：管理员手机客户端。
- `charging_server`：后台TCP、业务和数据库服务。
- `protocol_tests`：公共消息协议自动测试。

当前版本已完成项目框架和第一阶段账户认证功能：

- 用户端使用11位手机号免密登录；手机号不存在时由服务端自动注册。
- 新用户默认昵称为“用户+手机号后4位”，使用灰色默认头像，初始余额为0元。
- 管理员端使用独立账号密码登录，默认初始账号为 `admin / 123456`。
- 两个客户端均只通过TCP连接服务端，不直接访问SQLite。
- 服务端维护用户/管理员角色会话，拦截未登录访问、越权访问和重复请求。
- 客户端定时发送心跳，连接超时后按退避策略自动重连；服务端清理失效连接。
- 登录和资料查询由Qt线程池处理，每个工作线程使用独立SQLite连接。
- 管理员密码使用随机盐和迭代SHA-256摘要保存，数据库中没有明文密码。

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
bash ./scripts/build.sh
```

编译结果位于 `build/bin/`。

## 运行协议测试

```bash
cd ~/charging-platform/build
./bin/protocol_tests -v1
./bin/phase1_tests -v1
```

或在项目根目录执行 `bash ./scripts/run-tests.sh`。

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

管理员端也提供不依赖服务端业务接口的只读验收模式：

```bash
./bin/charging_admin_client --demo
```

该模式使用样例 JSON 展示运营概览、实时充电、告警、站点、电桩、用户和订单页面，
不会提交远程控制或数据修改。

同一虚拟机联调时，两个客户端均连接 `127.0.0.1:8888`。

用户端输入一个以1开头的11位手机号即可登录；首次使用该号码会自动注册。管理员端首次运行使用 `admin / 123456` 登录。默认账号只用于课程演示，正式部署前应增加修改密码功能并替换默认密码。
