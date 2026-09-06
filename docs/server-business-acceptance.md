# 服务端业务联调与验收

确认日期：2026-09-06。范围以外层 `分工.md` 中易子恒任务为准，基线为
`origin/feat/database-repositories`，实现位于 `feat/yiziheng-server-core`。
协议确定值、字段、枚举、错误码、管理别名和成功/失败报文见
[已填写接口确认单](../user-client-server-confirmation-request.md)。

## 启动与种子数据

需要 Qt Core、Gui（头像解码）、Network、Sql、Concurrent、Test 和 SQLite 驱动，C++17。
两个客户端还需要 Widgets、Charts、WebEngineWidgets。Ubuntu 安装和整项目构建见 README。
以下命令均在仓库根目录运行；macOS 使用 qmake，发行版可能名为 qmake6。

仅构建服务端和全部五个测试目标，不依赖客户端 WebEngine：

```bash
mkdir -p build/server build/protocol-tests build/phase1-tests \
  build/database-repository-tests build/service-tests build/business-integration-tests
(cd build/server && qmake ../../server/server.pro CONFIG+=release CONFIG-=debug && make -j4)
for target in protocol-tests phase1-tests database-repository-tests service-tests business-integration-tests; do
  (cd "build/$target" && qmake "../../tests/$target.pro" CONFIG+=release CONFIG-=debug && make -j4) || exit 1
done
bash scripts/run-tests.sh
```

`scripts/run-tests.sh` 当前在 Git 中没有可执行位，使用 `bash` 调用。清洁构建时在每个
`build/<target>` 下先运行 `make clean`，再执行相同 qmake/make 命令。调试版将 CONFIG 参数换成
`CONFIG+=debug CONFIG-=release`。macOS 可附加 `-spec macx-clang`，Linux 不应指定此 spec。

```bash
cd build
./bin/charging_server --port 8888 --database data/charging.db
```

默认监听所有地址、端口 8888，SQLite 默认路径为工作目录下的 `data/charging.db`；头像在数据库目录的
`avatars/` 下。CLI 可用 `-p/--port`（1–65535）、`-d/--database`、`--help`、`--version`。
启动自动迁移并检查数据库，初始化管理员并恢复 charging 订单。默认管理员 `admin / 123456`，
仅用于课程联调。新数据库只初始化站点、电桩及管理员，没有预置用户、钱包、预约、订单、收藏或告警。

| 站点 ID | 名称与地址 | latitude / longitude（GCJ-02） | 单价 cents/kWh |
|---|---|---|---|
| 1 | 软件园充电站，大连市高新园区软件园路 | 38.8584 / 121.5312 | 120 |
| 2 | 星海充电站，大连市沙河口区星海广场 | 38.8817 / 121.5868 | 138 |

| 桩 ID | 站点 ID | pileCode | type / powerKw | 数据库/公开状态 |
|---|---|---|---|---|
| 1 | 1 | DL-SP-001 | fast / 60 | idle / available |
| 2 | 1 | DL-SP-002 | slow / 7 | idle / available |
| 3 | 2 | DL-XH-001 | fast / 120 | idle / available |

种子定义在 `database/seed.sql`，使用 INSERT OR IGNORE，已有数据不强制重置为种子状态。
默认每秒采样充电、每 60 秒清理到期预约、每 10 秒检查连接；超过 45 秒无活动的 session 被清理。
请求处理超时为 10 秒。传感器默认模拟已连接、25°C、400V 额定电流、60kWh 充满。

## 可复现联调流程

1. 启动服务端；用户连接 `127.0.0.1:8888`，发送 UserLoginRequest，phone=`13800138000`。
   首次创建用户，nickname=`用户8000`、avatar/avatarPath=`default://gray-avatar`、balanceCents=0；记录实际 userId。
2. 另一连接用 `admin/123456` 登录管理员；登录后才能收到充电/告警推送。
3. 用户以独立 transactionId（例如 `demo-recharge-001`）充值 10000 cents。相同交易内容重放后余额仍 10000；
   修改金额重用该 ID 返回 transaction_conflict。已有数据库请使用新的交易号或核对原流水。
4. 查询站点 1 和电桩，收藏站点，选择空闲桩 1 预约 15 分钟。记录实际 reservationId；
   另一用户也必须先登录并充值，再抢同一桩，预期 pile_unavailable。
5. 开始充电，记录 orderId。活动订单查询 active=true；用户与管理员收到同一订单严格递增 seq 的进度。
   60kW、120 cents/kWh 在 60 秒时累计约 1000Wh、120 cents，以服务端实际采样为准。
6. 用户停止或管理员 pile.stop，核对 completed、stopReason、最终 feeCents 和 balanceCents；
   收到一次停止事件。重试停止不再次扣费；活动订单查询变为 active=false；流水新增 charge_payment。
7. 另建预约并取消，确认桩释放；对空闲桩停用后不可预约，启用后可用。冻结用户后新预约失败，
   解冻恢复；已有充电订单不会因冻结立即停止。
8. 在自动化测试中使用可注入时钟/传感器或测试库设备状态覆盖故障与过期。
   公共 TCP 协议没有“注入故障”命令，不能将测试钩子当客户端 API。

## 需求到测试函数

矩阵中的 UI 跳转、提示、隐藏和真实设备响应由相应客户端/设备模块承担；以下对应服务端可验证行为。
`S` 表示 `tests/service_test.cpp`，`I` 表示 `tests/business_integration_test.cpp`，
`D` 表示 `tests/database_repository_test.cpp`，`P` 表示 `tests/protocol_test.cpp`。

| 需求 | 服务端行为 | 对应测试函数 |
|---|---|---|
| 19 未完成订单检查 | 活动订单快照/空结果、归属 | S: activeOrderAndSettlement；I: userWorkflowAndPushes |
| 20 充电桩预约 | 空闲选择、时长、取消/到期、并发互斥 | S: reservationSelectionAndDuration, reservationValidationAndConflicts, reservationCancelAndExpiry, simultaneousReservations, simultaneousDirectOrderAndReservation；I: reservationExpiryJob |
| 21 开始充电 | 预约/余额/冻结/桩校验、幂等、事务内重检 | S: chargingValidationAndOwnership, chargingStartRechecksEligibilityAtWrite, chargingProgressAndIdempotentStop |
| 22 实时充电信息 | 电量/功率/时长/费用/状态、更新时间 | S: chargingProgressAndIdempotentStop；I: userWorkflowAndPushes |
| 23 充电费用计算 | 整数 Wh/cents，单价快照，舍入和上溢 | S: billingRoundingAndValidation, chargingRestoreAndPersistedProgress；D: chargingSettlementAndRollback |
| 24 结束与结算 | 扣款、订单和释放原子提交，重复停止一致 | S: activeOrderAndSettlement, settlementFailureRollsBack, chargingSettlementRetryAndAlarmDeduplication；D: atomicSettlementAndRecovery |
| 25 充电提醒与异常 | 充满、余额不足、设备异常/断线自动终止 | S: chargingAutomaticStops, chargingRestoreUnavailablePile, chargingNormalStopRetryAfterPileFailure；I: faultsReachOwnerAndAdministrators |
| 35 定时推送 | 用户/管理员接收、序列跨重启、失败不伪造进度 | S: chargingSequenceSurvivesRestart, chargingProgressFailureDoesNotPublish；I: userWorkflowAndPushes, directPeerStreamRejectsReordering, directPeerStreamsRejectSharedGap, oversizedChargingPushIsAudited |
| 37 充满与故障告警 | 阈值检测、未恢复类型去重、故障结算 | S: chargingAutomaticStops, chargingSettlementRetryAndAlarmDeduplication；I: faultsReachOwnerAndAdministrators |
| 38 远程停止与计费 | 管理员复用结算，审计失败重试绑定原订单 | S: adminRemoteStopAndFreeze, adminStopRetryAfterAuditFailureKeepsOriginalOrder；I: userWorkflowAndPushes |
| 39 控制审计 | 管理员电桩控制尝试、结果、目标及请求号 | S: adminControlConflictsAndAudits, adminControlReplayIsScopedToConnection, adminDeviceChangeRollsBackWithAudit；D: alarmControlAndPushRecords |
| 46 远程重启 | 状态互斥、会话范围重放；通用请求超时 | S: adminControlReplayAndClientMetadata, adminRestartRejectsEveryActiveReservation；I: directTimeoutKeepsRequestReserved |
| 47 远程启用/停用 | 与活动预约/订单互斥、持久状态更新 | S: adminControlConflictsAndAudits, adminDeviceChangeRollsBackWithAudit, adminRemoteStopAndFreeze |
| 分工扩展：资料/头像 | 校验、独立字段并发更新、原子落盘与回滚 | S: profileValidationAndPartialUpdate, concurrentIndependentProfileUpdates, profileResponseReflectsCommittedRow, avatarValidationAndPersistence, avatarDatabaseFailureKeepsOldFile |
| 分工扩展：钱包 | 充值边界、幂等、分页/排序 | S: rechargeBoundsAndIdempotency, walletPaginationAndOrdering；D: rechargeIdempotency |
| 分工扩展：站点/电桩/收藏 | 坐标/文本/排序、状态映射、收藏目标幂等 | S: nearbyTextSearchAndCoordinates, pileStatusAndFavoriteIdempotency |
| 分工扩展：管理查询/维护 | 统计、站点、用户冻结、订单/CSV、告警 | S: adminQueriesStatisticsAndExport, adminMaintenanceAndAliases, adminAlarmDetailSurvivesListFilters, adminRepositorySnapshotsSurviveConcurrentChanges, adminStatisticsRejectsOverflow, adminExportHonorsEncodedTransportLimit, adminExportKeepsLargeIntegers |
| 分工扩展：协议/鉴权/超时 | 类型映射、session 归属、重复、超时、登录状态代次、帧边界 | P: businessProtocolRoundTrip；I: unauthorizedResponses, roleGuards, invalidFields, duplicateRequestLifecycle, directRequestMappingsAndLifecycle, directTimeoutKeepsRequestReserved, directAdminScopeAndDisconnect, directLoginCannotSurviveAuthenticationAba, directAnonymousClearInvalidatesLogin, directOutboundEnvelopeBoundaries |
| 分工扩展：数据库失败/回滚 | 业务错误统一，不发布未落库结果 | S: databaseFailures, reservationWriteFailureRollsBack, settlementFailureRollsBack, adminValidationAndDatabaseFailures |

需求边界：第 23 项目前为固定单价，没有分时电价。第 37 项没有自动恢复通知，充满由停止推送表示。
第 38/46 项为模拟设备状态与计费控制，没有真实设备 ACK。
第 39 项 device_control_records 目前记录管理员电桩控制；用户开始/停止与系统自动停止留存在订单/钱包/告警中，
尚非所有来源统一的逐条控制审计。第 47 项查询仍返回不可用状态，客户端负责展示与选择限制。

订单从 charging 开始创建；此前的 reserved 是桩状态，预约本身为 active。
stopping 只存在于运行时，订单表允许 charging/completed/fault_stopped/cancelled。
故障结算目前将 charging/fault/offline 的桩恢复为 idle（公开 available），其他状态保持原值；
这是实现限制，异常结束不保证桩继续处于故障/离线隔离状态。
pile.restart/enable/disable 的设备状态与最终审计结果在同一事务中提交；pile.stop 的结算先于最终审计，
审计失败可在已扣款、订单已结束之后返回 DatabaseError。原会话范围、请求号、action 和目标的重试
保持绑定首次订单，不会误停同桩后续订单。此行为由 adminStopRetryAfterAuditFailureKeepsOriginalOrder 覆盖。

## 本次验证记录

环境：macOS 26.5.1 arm64、Qt 6.11.1、Apple clang 21、SQLite Qt 驱动。
执行日期：2026-09-06。

- 尝试顶层 `qmake charging-platform.pro -spec macx-clang CONFIG+=debug && make -j4`：
  在 user-client 配置因本机缺少 `webenginewidgets` 停止。这是本地依赖造成的整项目验证限制，
  不是产品不支持客户端或 WebEngine。
- 六个独立目标（server 和五套测试）debug 配置构建成功，`bash scripts/run-tests.sh` 成功退出。
- 六个目标分别 `make clean`，重新 qmake release 并 `make -j4`；构建日志无 warning/error。
- release 全量测试成功退出，结果如下；真实 TCP 测试需要能绑定本机临时端口。

| 套件 | 通过 | 失败 | 跳过 |
|---|---:|---:|---:|
| protocol_tests | 11 | 0 | 0 |
| phase1_tests | 7 | 0 | 0 |
| database_repository_tests | 34 | 0 | 0 |
| service_tests | 51 | 0 | 0 |
| business_integration_tests | 56 | 0 | 0 |

计数包含 QtTest 初始化/清理及数据驱动用例。测试使用独立临时数据库，不依赖手工联调库。
本地完整 release 日志为 `build/task-8-release-tests.log`，不提交构建产物。
