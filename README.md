# 电动汽车充电桩应用管理平台

## 项目组成

- `charging_user_client`：用户手机客户端。
- `charging_admin_client`：管理员手机客户端。
- `charging_server`：后台TCP、业务和数据库服务。
- `protocol_tests`：公共消息协议自动测试。
- `phase1_tests`、`database_repository_tests`、`service_tests`、`business_integration_tests`：认证、数据库、业务和 TCP 集成回归。

当前分支包含账户认证、数据库事务和服务端业务核心：

- 用户端使用11位手机号免密登录；手机号不存在时由服务端自动注册。
- 新用户默认昵称为“用户+手机号后4位”，使用灰色默认头像，初始余额为0元。
- 管理员端使用独立账号密码登录，默认初始账号为 `admin / 123456`。
- 两个客户端均只通过TCP连接服务端，不直接访问SQLite。
- 服务端维护用户/管理员角色会话，拦截未登录访问、越权访问和重复请求。
- 客户端定时发送心跳，连接超时后按退避策略自动重连；服务端清理失效连接。
- 登录和资料查询由Qt线程池处理，每个工作线程使用独立SQLite连接。
- 管理员密码使用随机盐和迭代SHA-256摘要保存，数据库中没有明文密码。
- 数据库按版本执行迁移，并在启动和恢复后执行完整性检查。
- Repository覆盖用户、管理员、站点、电桩、收藏、预约、订单、钱包流水、告警、设备控制和推送记录。
- 充值、预约、开始充电、订单结算和电桩释放使用事务与条件更新保证一致性。
- 支持SQLite在线快照备份、备份完整性校验和恢复。
- 用户业务覆盖资料/头像、钱包充值与流水、附近站点、电桩、收藏、预约与取消、活动订单和开始/停止充电。
- 充电模拟器每秒采样计费，向用户和管理员推送进度、停止结果及故障告警；支持活动订单重启恢复。
- 管理业务覆盖统计、站点维护、电桩控制及审计、用户冻结、订单查询、告警处理和 CSV 导出，保留管理端 action 别名。

两个客户端的联调依据是[已填写接口确认单](user-client-server-confirmation-request.md)，
启动参数、种子数据、验收步骤、需求到测试的映射及实现边界见[服务端验收说明](docs/server-business-acceptance.md)。
当前设备数据和控制使用模拟器；头像返回服务器相对路径，未提供 HTTP 下载接口。

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

## 运行测试

```bash
cd ~/charging-platform/build
./bin/protocol_tests -v1
./bin/phase1_tests -v1
./bin/database_repository_tests -v1
./bin/service_tests -v1
./bin/business_integration_tests -v1
QT_QPA_PLATFORM=offscreen ./bin/user_ui_tests -v1
./bin/admin_controller_tests -v1
QT_QPA_PLATFORM=offscreen ./bin/admin_ui_tests -v1
```

或在项目根目录执行 `bash ./scripts/run-tests.sh`。测试会创建临时数据库并监听本机临时 TCP 端口。
只验证服务端时，可按[独立目标构建步骤](docs/server-business-acceptance.md#启动与种子数据)构建六个目标。

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

明天演示时可直接启动用户端完整体验模式，不依赖尚未完成的业务接口：

```bash
./bin/charging_user_client --demo
```

真实地址搜索需要腾讯位置服务 WebService Key。复制 `config/app.ini.example` 为
`config/app.ini` 并填写 `[map]` 下的 `tencent_key`，或在启动前设置
`TENCENT_MAP_KEY`。Linux 客户端启动时会在未指定其他输入法框架的情况下自动连接
IBus，以支持地址框中文输入。

演示顺序为“搜索附近站点 → 查看电桩 → 收藏或导航 → 预约 → 开始充电 →
观察实时电量/功率/费用 → 停止并结算”。“我的”页面还可演示头像与昵称维护、
钱包充值、充值记录和常用站点。页面会明确标注演示数据，服务端到位后仍沿用
相同的页面和信号入口接入真实响应。

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

新数据库包含两个大连充电站和三个空闲桩，没有预置用户或订单。首次用户余额为 0，需先调用充值接口，
再预约并开始充电。停止成功即已结算扣款；重连或请求超时后查询活动订单确认状态。
