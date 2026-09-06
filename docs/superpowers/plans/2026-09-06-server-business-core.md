# Server Business Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成易子恒负责的服务端协议、业务接口、实时充电状态机、告警推送和管理员控制，并提供可重复执行的自动化测试。

**Architecture:** `MessageDispatcher` 只处理鉴权、参数和响应，业务规则放在按领域拆分的 Service 中，所有持久化通过 Repository 完成。`ChargingService` 维护活动充电快照，由 `JobManager` 每秒驱动，`ServerApplication` 负责在线会话路由和推送。

**Tech Stack:** C++17、Qt Core/Network/SQL/Concurrent/Test、QSQLite、qmake

**Spec:** `docs/superpowers/specs/2026-09-06-server-business-core-design.md`

## Global Constraints

- 基线为 `origin/feat/database-repositories`，工作分支为 `feat/yiziheng-server-core`。
- 协议版本保持 1；请求 `requestId` 非 0，响应必须原样返回。
- 金额使用整数 cents，数据库电量使用整数 Wh，时间使用 UTC ISO 8601。
- 用户身份只取已认证 session；管理员业务必须验证 Administrator 角色。
- 不直接在 Service 中持有跨线程 QSqlDatabase；每次调用后释放工作线程连接。
- 使用 TDD：每个生产行为先写失败测试并确认失败，再写最小实现。

---

### Task 1: 扩展公共消息协议和业务结果类型

**Files:**
- Modify: `common/protocol/messagetypes.h`
- Modify: `common/protocol/errorcodes.h`
- Modify: `common/protocol/errorcodes.cpp`
- Create: `server/services/serviceresult.h`
- Modify: `tests/protocol_test.cpp`

**Interfaces:**
- Produces: `ServiceResult { ErrorCode error; QString message; QString reason; QJsonObject payload; bool succeeded() const; }`
- Produces: 新消息枚举 1020/1021、1100/1101、1110/1111、2020/2021、3030/3031、3040/3041。

- [ ] **Step 1: 写失败的协议枚举与错误负载测试**

在 `tests/protocol_test.cpp` 增加断言，验证 `WalletRechargeRequest == 1100`、
`ReservationCancelResponse == 3041`，并编码/解码一个带 `reason=insufficient_balance` 的
`Conflict` 响应，断言 requestId、statusCode 和负载保持不变。

- [ ] **Step 2: 运行测试并确认失败**

Run: `qmake charging-platform.pro -spec macx-clang CONFIG+=debug && make -j4 && build/bin/protocol_tests -v1`
Expected: 编译失败，提示新增枚举尚未定义。

- [ ] **Step 3: 添加最小协议定义**

按设计文档写入枚举；创建可由所有 Service 复用的 `ServiceResult`，其 `succeeded()` 仅在
`error == ErrorCode::Success` 时返回 true。保留现有错误码数值以兼容客户端。

- [ ] **Step 4: 运行协议测试**

Run: `make -j4 && build/bin/protocol_tests -v1`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add common/protocol tests/protocol_test.cpp server/services/serviceresult.h
git commit -m "feat: extend business message protocol"
```

### Task 2: 补足事务型 Repository 接口

**Files:**
- Modify: `server/repositories/userrepository.h`
- Modify: `server/repositories/userrepository.cpp`
- Modify: `server/repositories/walletrepository.h`
- Modify: `server/repositories/walletrepository.cpp`
- Modify: `server/repositories/reservationrepository.h`
- Modify: `server/repositories/reservationrepository.cpp`
- Modify: `server/repositories/orderrepository.h`
- Modify: `server/repositories/orderrepository.cpp`
- Modify: `server/repositories/stationrepository.h`
- Modify: `server/repositories/stationrepository.cpp`
- Modify: `server/repositories/pilerepository.h`
- Modify: `server/repositories/pilerepository.cpp`
- Modify: `tests/database_repository_test.h`
- Modify: `tests/database_repository_test.cpp`

**Interfaces:**
- Produces: `WalletRepository::findByRecordNo(const QString &, WalletRecord *, QString *) const`
- Produces: `WalletRepository::countByUser(qint64, int *, QString *) const`
- Produces: `ReservationRepository::findById(qint64, ReservationRecord *, QString *) const`
- Produces: `OrderRepository::stopAndSettle(qint64 orderId, qint64 durationSeconds, qint64 energyWh, qint64 feeCents, const QString &finalStatus, const QString &reason, qint64 *balanceAfterCents, QString *error) const`
- Produces: `OrderRepository::listActive(QList<OrderRecord> *, QString *) const`
- Produces: 列表 count 方法及订单管理分页方法。

- [ ] **Step 1: 写事务和幂等失败测试**

增加测试：相同 `recordNo` 充值两次只增加一次余额；结算时同时更新订单、钱包和桩；余额
不足时事务整体回滚；预约取消/过期释放桩；活动订单可在服务重启时列出。

- [ ] **Step 2: 运行数据库测试并确认失败**

Run: `qmake charging-platform.pro -spec macx-clang CONFIG+=debug && make -j4 && build/bin/database_repository_tests -v1`
Expected: 编译失败，提示上述接口未定义。

- [ ] **Step 3: 实现最小 Repository 扩展**

复用 `DatabaseManager::transaction` 约定和现有命名参数查询。`stopAndSettle` 在一个事务中
校验活动订单、检查余额、写负数钱包流水、更新订单终态、累计桩使用量并将桩释放为 idle。
相同 recordNo 返回已存在流水的余额，不再次执行 UPDATE。

- [ ] **Step 4: 运行数据库测试**

Run: `make -j4 && build/bin/database_repository_tests -v1`
Expected: PASS，且故意触发的失败事务后各表状态未改变。

- [ ] **Step 5: 提交**

```bash
git add server/repositories tests/database_repository_test.*
git commit -m "feat: add transactional business repositories"
```

### Task 3: 用户资料、钱包、站点、电桩和收藏服务

**Files:**
- Modify: `server/services/userservice.h`
- Create: `server/services/userservice.cpp`
- Modify: `server/services/stationservice.h`
- Create: `server/services/stationservice.cpp`
- Modify: `server/services/pileservice.h`
- Create: `server/services/pileservice.cpp`
- Create: `tests/service_test.h`
- Create: `tests/service_test.cpp`
- Create: `tests/service-tests.pro`
- Modify: `charging-platform.pro`
- Modify: `scripts/run-tests.sh`

**Interfaces:**
- Produces: `UserService::updateProfile(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `UserService::recharge(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `UserService::walletLedger(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `StationService::nearby(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `StationService::toggleFavorite(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `PileService::listForStation(const QJsonObject &) -> ServiceResult`

- [ ] **Step 1: 写服务失败测试**

覆盖昵称字符与长度、空更新、头像格式/2 MiB 限制、充值上下限与 transactionId 幂等、流水
倒序分页、无坐标站点搜索、Haversine 半径/距离排序、idle 到 available 映射及收藏幂等。

- [ ] **Step 2: 运行测试并确认失败**

Run: `qmake tests/service-tests.pro -o build/service-tests/Makefile && make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: 编译失败，提示服务方法未定义。

- [ ] **Step 3: 实现服务**

每个入口先校验 JSON 类型和值域，再调用 Repository；数据库错误统一返回 `DatabaseError`。
头像先由 `QImageReader` 验证，再写临时文件并原子重命名。站点距离使用 double 计算，仅展示
时保留两位小数；返回原始经纬度。

- [ ] **Step 4: 运行服务与数据库测试**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1 && build/bin/database_repository_tests -v1`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add server/services tests/service* charging-platform.pro scripts/run-tests.sh
git commit -m "feat: implement user and station services"
```

### Task 4: 预约、订单、计费服务与正常状态流转

**Files:**
- Modify: `server/services/reservationservice.h`
- Create: `server/services/reservationservice.cpp`
- Modify: `server/services/orderservice.h`
- Create: `server/services/orderservice.cpp`
- Modify: `server/services/billingservice.h`
- Create: `server/services/billingservice.cpp`
- Modify: `tests/service_test.h`
- Modify: `tests/service_test.cpp`

**Interfaces:**
- Produces: `ReservationService::create(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `ReservationService::cancel(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `ReservationService::expireDue(const QDateTime &) -> ServiceResult`
- Produces: `OrderService::active(qint64) -> ServiceResult`
- Produces: `BillingService::feeCents(qint64 energyWh, qint64 priceCentsPerKwh) -> qint64`
- Produces: `BillingService::settle(...) -> ServiceResult`

- [ ] **Step 1: 写失败测试**

覆盖自动选择空闲桩、显式 pileId、一个用户一个活动预约、冻结/零余额/过期/冲突、取消与过期
释放电桩、无活动订单返回 `{active:false}`，以及 `floor(Wh * centsPerKwh / 1000)` 计费。

- [ ] **Step 2: 运行并确认失败**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: 编译失败，提示预约和订单服务方法未定义。

- [ ] **Step 3: 实现最小业务流**

预约时长默认 15 分钟、范围 5–30 分钟；所有权和用户状态在 Service 校验，竞争条件由数据库
唯一索引和事务兜底。订单 JSON 使用 `payableCents` 暴露当前费用。

- [ ] **Step 4: 运行测试**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add server/services tests/service_test.*
git commit -m "feat: implement reservation and billing services"
```

### Task 5: 实时充电状态机、恢复、停止和告警

**Files:**
- Modify: `server/services/chargingservice.h`
- Create: `server/services/chargingservice.cpp`
- Modify: `server/services/alarmservice.h`
- Create: `server/services/alarmservice.cpp`
- Modify: `server/services/serviceregistry.h`
- Modify: `server/services/serviceregistry.cpp`
- Modify: `server/jobs/jobmanager.h`
- Modify: `server/jobs/jobmanager.cpp`
- Modify: `tests/service_test.h`
- Modify: `tests/service_test.cpp`

**Interfaces:**
- Produces: `ChargingService::start(qint64, const QJsonObject &) -> ServiceResult`
- Produces: `ChargingService::stop(qint64 actorId, Role, const QJsonObject &) -> ServiceResult`
- Produces: `ChargingService::tick(const QDateTime &) -> QList<ChargingEvent>`
- Produces: `ChargingService::restore() -> ServiceResult`
- Produces: `ChargingEvent { MessageType type; qint64 userId; QJsonObject payload; }`
- Produces: `AlarmService::raiseOnce(...) -> ServiceResult`

- [ ] **Step 1: 写状态机失败测试**

使用可注入时间连续 tick，断言 seq 递增、时长/Wh/费用基于总量计算；测试重复开始/停止、
余额不足、充满、fault、offline、温度 60°C 以上、电流异常和断线只告警一次并结算；测试从
数据库恢复 charging 订单。

- [ ] **Step 2: 运行并确认失败**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: 编译失败，提示充电接口和 `ChargingEvent` 未定义。

- [ ] **Step 3: 实现状态机**

以订单 ID 为 key 保存活动快照；tick 从 `startedAt` 计算总时长，按额定功率算总 Wh，避免
重复 tick 双计费。停止先标记 stopping，再调用事务结算；只有结算成功才移除活动快照。
JobManager 增加 reservationExpiryTick（60 秒）并保留 chargingTick（1 秒）。

- [ ] **Step 4: 运行服务测试和 AddressSanitizer 可用构建**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: PASS，无重复结束事件。

- [ ] **Step 5: 提交**

```bash
git add server/services server/jobs tests/service_test.*
git commit -m "feat: add realtime charging state machine"
```

### Task 6: 管理查询、维护、控制和审计服务

**Files:**
- Modify: `server/services/adminservice.h`
- Create: `server/services/adminservice.cpp`
- Modify: `server/services/statisticsservice.h`
- Create: `server/services/statisticsservice.cpp`
- Modify: `server/repositories/alarmrepository.h`
- Modify: `server/repositories/alarmrepository.cpp`
- Modify: `tests/service_test.h`
- Modify: `tests/service_test.cpp`

**Interfaces:**
- Produces: `AdminService::execute(qint64 adminId, quint32 requestId, const QJsonObject &, ChargingService *) -> ServiceResult`
- Produces: `StatisticsService::summary(const QJsonObject &) -> ServiceResult`
- Consumes: Task 5 的 `ChargingService::stop`。

- [ ] **Step 1: 写 action 分发失败测试**

逐个覆盖设计文档中的 action，重点断言分页字段、统计合计、冻结限制、活动桩禁止停用/重启、
远程停止复用一次结算，以及成功/失败控制均写审计记录。CSV 返回带 BOM 内容和 `.csv` 文件名。

- [ ] **Step 2: 运行并确认失败**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: 编译失败，提示 `AdminService::execute` 未定义。

- [ ] **Step 3: 实现管理业务**

使用 action 到私有处理函数的显式映射；未知 action 返回 `UnsupportedMessage`。所有列表限制
`page >= 1`、`1 <= pageSize <= 100`。冻结不终止已有订单；停用和重启先查桩及活动订单。

- [ ] **Step 4: 运行服务测试**

Run: `make -C build/service-tests -j4 && build/bin/service_tests -v1`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add server/services server/repositories/alarmrepository.* tests/service_test.*
git commit -m "feat: implement admin operations and statistics"
```

### Task 7: 注册消息处理器并接通在线推送

**Files:**
- Modify: `server/dispatch/messagedispatcher.h`
- Modify: `server/dispatch/messagedispatcher.cpp`
- Modify: `server/network/tcpserver.h`
- Modify: `server/network/tcpserver.cpp`
- Modify: `server/app/serverapplication.h`
- Modify: `server/app/serverapplication.cpp`
- Modify: `server/server.pro`
- Create: `tests/business_integration_test.h`
- Create: `tests/business_integration_test.cpp`
- Create: `tests/business-integration-tests.pro`
- Modify: `charging-platform.pro`
- Modify: `scripts/run-tests.sh`

**Interfaces:**
- Produces: `TcpServer::sessionsForUser(qint64) -> QList<ClientSession *>`
- Produces: `TcpServer::administratorSessions() -> QList<ClientSession *>`
- Consumes: Tasks 3–6 的 Service 接口和 `ChargingEvent`。

- [ ] **Step 1: 写端到端失败测试**

启动临时服务器，登录用户和管理员；验证每个请求得到正确 response type/requestId；完成站点
查询、预约、开始、进度推送、主动停止、最终推送和活动订单查询；验证未登录、角色越权、
非法字段和未知 action；管理员与所属用户都收到告警。

- [ ] **Step 2: 运行并确认失败**

Run: `qmake tests/business-integration-tests.pro -o build/business-integration-tests/Makefile && make -C build/business-integration-tests -j4`
Expected: 编译或测试失败，因为新消息尚未注册。

- [ ] **Step 3: 实现分发和推送路由**

用一个通用异步执行 helper 保证 mark/finish 成对调用并统一插入 message/reason；请求映射到
固定响应类型。连接关闭时在线查询自然排除该 session。ServerApplication 将 chargingTick
连接到 `ChargingService::tick` 并按事件目标发送推送。

- [ ] **Step 4: 运行集成测试**

Run: `make -C build/business-integration-tests -j4 && build/bin/business_integration_tests -v1`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add server/dispatch server/network server/app server/server.pro tests/business* charging-platform.pro scripts/run-tests.sh
git commit -m "feat: wire business handlers and charging pushes"
```

### Task 8: 联调文档、示例报文和全量回归

**Files:**
- Modify: `user-client-server-confirmation-request.md`（仓库根目录新增副本）
- Create: `docs/server-business-acceptance.md`
- Modify: `README.md`

**Interfaces:**
- Documents: 所有枚举、字段、reason、action、分页、推送频率、种子数据和启动方式。

- [ ] **Step 1: 先运行全量测试并记录任何失败**

Run: `qmake charging-platform.pro -spec macx-clang CONFIG+=debug && make -j4 && scripts/run-tests.sh`
Expected: 所有既有和新增测试 PASS；若失败，只修复本分支引入的回归后重新运行。

- [ ] **Step 2: 填写联调确认单**

将外层确认单复制进仓库并填写确定值；为资料、充值、站点、电桩、收藏、预约、活动订单、
开始/停止、进度/停止推送和管理员 action 各提供成功与失败 JSON 示例。

- [ ] **Step 3: 编写验收说明**

记录 `admin/123456`、seed 中用户/站点/桩、服务器默认配置、测试命令以及需求 19–25、35、
37–39、46–47 和 `分工.md` 扩展接口到测试函数的对应关系。

- [ ] **Step 4: 清洁构建验证**

Run: `make clean && qmake charging-platform.pro -spec macx-clang CONFIG+=release && make -j4 && scripts/run-tests.sh`
Expected: release 构建成功，全量测试 PASS，输出无编译警告或测试跳过。

- [ ] **Step 5: 检查分支改动并提交**

Run: `git diff --check && git status --short && git log --oneline origin/feat/database-repositories..HEAD`
Expected: 无空白错误，仅包含本任务文件。

```bash
git add README.md user-client-server-confirmation-request.md docs/server-business-acceptance.md
git commit -m "docs: add server integration contract"
```
