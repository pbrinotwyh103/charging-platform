# 服务端业务、通信接口与实时充电核心设计

## 目标与范围

在 `feat/yiziheng-server-core` 分支完成易子恒负责的服务端功能，并以
`origin/feat/database-repositories` 提供的数据访问接口为基础。实现范围以仓库外层
`分工.md` 为准，覆盖公共业务协议、用户业务接口、充电状态机与定时推送、异常告警、
管理员控制和管理查询，以及对应服务层与集成测试。

本分支不实现用户端或管理员端界面，不绕过 Repository 直接拼接业务 SQL，也不修改
数据库备份恢复等其他成员负责的功能。若现有 Repository 缺少完成原子业务所需的最小
接口，只补充必要接口并保持数据库模块的既有风格。

## 开发基线与兼容目标

- 开发基线：`origin/feat/database-repositories`。
- 用户端兼容：`origin/syh-user-client` 已使用的 `StationListRequest`、
  `PileListRequest` 和资料读取报文继续可用。
- 管理端兼容：`origin/admin-client` 使用统一 `AdminCommandRequest`，请求体中的
  `action` 决定业务，响应统一为 `AdminCommandResponse`。
- 沿用协议头版本 1，所有客户端请求的 `requestId` 必须非 0，响应原样返回。
- 金额使用整数 `cents`，电量在持久层使用整数 `Wh`，展示和推送时转换为 `kWh`。
- 时间使用 UTC ISO 8601（带 `Z`），坐标使用 GCJ-02，经纬度字段为 `latitude`、
  `longitude`。

## 公共协议

在 `common/protocol/messagetypes.h` 中保留已有枚举值，并新增以下成对类型：

| 功能 | 请求 | 响应 | 枚举值 |
|---|---|---|---|
| 资料更新 | `UserProfileUpdateRequest` | `UserProfileUpdateResponse` | 1020 / 1021 |
| 钱包充值 | `WalletRechargeRequest` | `WalletRechargeResponse` | 1100 / 1101 |
| 充值流水 | `WalletLedgerRequest` | `WalletLedgerResponse` | 1110 / 1111 |
| 收藏切换 | `FavoriteToggleRequest` | `FavoriteToggleResponse` | 2020 / 2021 |
| 活动订单 | `ActiveOrderRequest` | `ActiveOrderResponse` | 3030 / 3031 |
| 取消预约 | `ReservationCancelRequest` | `ReservationCancelResponse` | 3040 / 3041 |

现有 2001/2002、2010/2011、3001/3002、3010/3011、3020/3021、8001/8002
保持不变。

统一响应由协议头 `statusCode` 表示结果，负载总是包含可展示的 `message`。成功响应包含
对应业务数据；失败响应不伪造成功数据。通用错误沿用 `Unauthorized`、`Forbidden`、
`ValidationFailed`、`Conflict`、`NotFound`、`DuplicateRequest`、`DatabaseError`、
`RequestTimeout` 和 `InternalError`。业务冲突通过稳定的负载字段 `reason` 细分，例如
`user_frozen`、`pile_unavailable`、`reservation_expired`、`insufficient_balance`、
`order_not_active` 和 `command_conflict`。

## 请求幂等与超时

会话层继续拒绝同一连接中仍在处理的重复 `requestId`。对于会改变资金或状态的业务，
仅依赖连接级 requestId 不足，因此同时使用业务键：充值使用 `transactionId`，订单使用
生成后唯一的 `orderNo`，管理控制记录保存 `requestId`。重复充值返回首次成功的同一流水
和余额，不重复入账；重复开始返回现有活动订单；重复停止返回已确定的最终订单结果。

分发器中的数据库业务继续通过线程池执行，完成后再回到对象线程发送响应。服务端超时
返回 `RequestTimeout(3002)`；客户端超时不能被解释为充电已经停止或失败。

## 用户业务接口

### 资料与钱包

- 资料更新请求允许 `nickname` 和 `avatarBase64` 二选一或同时提供，但不能同时为空。
- 昵称去除首尾空白后为 2–20 个字符，允许中文、字母、数字、空格、下划线和短横线，
  不要求全局唯一。
- 头像支持 JPEG 和 PNG，解码后最大 2 MiB；服务端校验 MIME 与实际图片格式，保存为
  服务端资源路径并返回 `avatar`。本阶段不做二次压缩。
- 资料响应返回 `userId`、`phone`、`nickname`、`avatar`、`balanceCents`、`status`。
- 充值范围为 100–1,000,000 cents，`transactionId` 必填且全局唯一。
- 流水按 `createdAt` 倒序分页，参数为 `page`、`pageSize`，响应为 `items`、`page`、
  `pageSize`、`total`；充值为正数，扣费为负数。

### 站点、电桩与收藏

- 附近站点请求接受 `region`、`address`、`latitude`、`longitude`、`radiusKm`、`sort`。
  `radiusKm` 默认为 10，范围为 1–100；有合法坐标时使用 Haversine 距离过滤和排序，
  无坐标时按站名、地址包含关系过滤。空结果返回成功和空 `items`。
- 站点项返回确认单要求的字段并附 `distanceKm`。收藏状态按当前 session 用户计算。
- 电桩请求以 `stationId` 查询，状态对外映射为 `available`、`reserved`、`charging`、
  `offline`、`fault`、`disabled`；其中数据库 `idle` 映射为 `available`。
- 收藏切换请求包含 `stationId`、`favorited`，重复提交返回目标最终状态而不报错。

### 预约、活动订单与充电

- 创建预约请求包含 `stationId`、可选 `pileId` 和可选 `durationMinutes`；默认 15 分钟，
  允许 5–30 分钟。未指定电桩时选择站内一个空闲桩。
- 同一用户只允许一个活动预约或活动订单；被冻结用户、余额为零、桩不可用或预约冲突
  返回 `Conflict` 并给出稳定 `reason`。
- 取消预约只能由预约所属用户执行。取消和过期都会释放仍处于 `reserved` 的电桩。
- 活动订单不存在时返回成功：`active: false`；存在时返回 `active: true` 和完整订单快照。
- 开始充电接受 `reservationId`，验证归属、有效期、用户状态、余额和桩状态后，将预约标记
  为已使用、创建订单并将桩切换为 `charging`。
- 停止请求接受 `orderId`。服务端先进入内存态 `stopping`，完成最终采样后调用数据库事务
  结算并释放电桩。响应和最终推送均返回订单最终状态、最终费用、余额和停止原因。

## 充电运行时与状态机

`ChargingService` 管理活动订单会话。预约发生在订单创建之前，必须区分各实体状态：

```text
预约：active -> used | cancelled | expired
电桩：idle -> reserved -> charging -> 结算释放
订单运行时：charging -> stopping -> completed | fault_stopped
```

订单在开始充电时创建，订单表仅允许 charging/completed/fault_stopped/cancelled；
reserved 不属于订单状态，stopping 仅为服务端运行时状态，不持久化到电桩、预约或订单表。
当前结算释放将 charging/fault/offline 的桩改为 idle，其他状态保持原值。
这是当前实现限制：故障/离线桩在异常结算后不会继续保持不可用状态。
每秒 tick 更新时长，每次按
电桩额定功率计算增量电量，并按 `feeCents = floor(energyWh * priceCentsPerKwh / 1000)`
计算累计费用。该规则确保计费只使用整数并且重复 tick 不会累计舍入误差。

每个订单维护独立且严格递增的 `seq`。每 1 秒向订单所属用户以及已认证管理员推送
`ChargingProgressPush`，包含 `orderId`、`seq`、`status`、`energyKwh`、`powerKw`、
`durationSec`、`feeCents`、`updatedAt`。停止后发送一次 `ChargingStoppedPush`；网络层允许
重复或延迟，但同一服务进程不会主动乱序发送。

运行时检查以下停止条件：费用达到余额、模拟电量达到充满阈值、设备状态变为 fault/offline、
温度超过 60°C、电流超过电桩额定功率对应上限、设备连接中断。异常在同一订单、同一类型
仍未恢复期间只创建和推送一次；严重异常触发停止，结算状态为 `fault_stopped`。

服务重启后从数据库恢复 `charging` 订单的累计量和开始时间，避免订单永久悬空。恢复时若
对应电桩不可用，则按连接中断异常结束。

## 管理业务

沿用统一 `AdminCommandRequest`。支持以下 action：

- `dashboard.summary`、`revenue.trend`、`charging.active.list`、
  `pile.status.summary`；
- `alarm.list`、`alarm.handle`；
- `station.list`、`station.detail`、`station.create`、`station.update`；
- `pile.list`、`pile.stop`、`pile.restart`、`pile.enable`、`pile.disable`；
- `user.list`、`user.freeze`、`user.unfreeze`；
- `order.list`、`report.export`。

列表接口使用 `page`、`pageSize`、`total`。冻结用户只限制新预约与新订单，不强制终止已在
充电的订单。停用电桩与活动预约或订单互斥；重启仅允许非充电桩；远程停止复用订单停止与
结算流程。所有远程控制记录操作者、目标、订单、请求号、结果和详情。

当前 device_control_records 覆盖管理员电桩控制尝试；用户及系统自动开始/停止信息保存在
订单、钱包和告警中，没有全部写为该表的逐条控制记录。重启/启用/停用的状态更新与最终审计
结果在同一事务提交。远程停止先提交订单结算，再完成最终审计；审计失败可能返回 DatabaseError，
但不回滚已完成的订单与扣款。原会话范围、requestId、action 和目标的重试仍绑定首次订单。

CSV 导出由服务端返回 UTF-8（带 BOM）的文本内容和建议文件名，不直接写入客户端文件系统。

## 组件边界与数据流

- `MessageDispatcher`：鉴权、角色校验、请求字段解析、调用服务、统一响应；不承载业务规则。
- `UserService`、`StationService`、`PileService`、`ReservationService`、`OrderService`、
  `BillingService`、`AlarmService`、`StatisticsService`、`AdminService`：各自封装一个业务域。
- `ChargingService`：协调预约、订单、计费、电桩和告警服务，并维护活动充电内存快照。
- `JobManager`：触发预约过期、充电 tick 和连接健康检查，不执行 SQL。
- Repository：唯一持久化入口；跨表状态改变通过数据库事务完成。
- ServerApplication/连接注册表：按用户 ID 或角色定位在线 session，负责实际推送。

## 错误恢复与一致性

任何跨预约、订单、钱包和电桩的写操作必须在同一数据库事务中完成。事务失败时返回
`DatabaseError`，不得只更新部分状态。运行时先依据数据库结果更新内存；若推送失败，只记
推送失败记录，不回滚已经完成的业务事务。未知 action 或消息类型返回
`UnsupportedMessage`，字段缺失或类型错误返回 `InvalidPayload` 或 `ValidationFailed`。

头像落盘采用临时文件后原子重命名；数据库更新失败时删除新文件并保留旧头像。

## 测试与验收

- 协议测试：新增枚举值不冲突，requestId 原样返回，响应消息类型正确，统一错误结构稳定。
- 服务测试：每个公开业务覆盖成功、校验失败、未找到、冻结、状态冲突和数据库失败。
- 幂等测试：重复充值、重复预约、重复开始、重复停止和重复管理员控制不产生双重副作用。
- 状态机测试：正常完成、主动停止、余额不足、充满、设备故障、温度/电流异常和断线。
- 计费测试：整数舍入、跨 tick 累计、恢复后续算及最终扣费一致。
- 集成测试：登录后完成预约到结算全流程，并验证用户和管理员收到正确推送。
- 管理测试：权限、分页、统计口径、冻结互斥、站点维护、电桩控制和审计记录。
- 全量执行 `scripts/run-tests.sh`，并单独构建 server、协议测试、服务测试和集成测试。

验收时同时填写 `user-client-server-confirmation-request.md`，提供成功、空数据、冲突、超时、
故障示例报文和种子数据说明，使两个客户端分支可直接联调。
