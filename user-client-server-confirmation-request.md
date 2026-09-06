# 用户端—服务端接口确认单（已填写）

本文件是外层同名确认单的仓库内完成版，确认日期为 2026-09-06。以本分支
`common/protocol/`、`server/services/`、`server/dispatch/` 的实现为准。
启动、种子数据和需求对应测试见 [服务端验收说明](docs/server-business-acceptance.md)。

## 一、公共协议

| 项目 | 服务端确认 |
|---|---|
| 传输与版本 | TCP；沿用 CHP1、版本 1。20 字节大端二进制头后跟 UTF-8 JSON 对象；头字段依次为 magic/u32、version/u16、messageType/u16、requestId/u32、payloadLength/u32、statusCode/i32。负载上限 4 MiB。使用公共 PacketCodec 编解码。 |
| requestId | 请求必须为非 0 的 u32，响应原样返回；推送为 0。消息类型值与错误码属于不同字段，例如预约响应类型 3002 不表示请求超时。 |
| 身份 | 用户身份来自认证 session；用户请求不用传 userId。管理 action 中的 userId 是被管理对象；操作者及内部 _requestScope 由服务端决定。重连后重新登录。 |
| 金额与电量 | 金额为整数 cents；持久化电量为整数 Wh，energyKwh = energyWh / 1000。ID 为正整数，输入上限 9007199254740991。 |
| 时间 | 返回 UTC ISO 8601，带 Z；可有毫秒。未发生的时间可为空字符串。日期筛选 from/to 为 YYYY-MM-DD。 |
| 坐标 | latitude/longitude，约定 GCJ-02；服务端不执行坐标系转换。 |
| 分页 | 流水、站点、电桩、管理列表用 page/pageSize/total/items；默认 1/20，pageSize 1–100；page 为正整数且 offset 不超过 int 最大值。越界页成功且 items=[]。管理列表另有 totalPages（空列表也为 1）和同值 meta。 |
| 结果 | 请求响应负载均有可展示 message，header.statusCode 决定成功或失败，reason 仅在有业务分类时出现。不要按中文 message 判断业务分支。推送不统一附加 message。 |
| 超时 | 分发器默认 10 秒，返回 RequestTimeout(3002)。超时不取消后台事务；同一 requestId 在任务实际完成前仍占用，迟到结果不再次发送。客户端应查询活动订单/流水确认状态。 |

连接中仍在执行的重复 requestId 返回 DuplicateRequest(1204)，完成后可复用，但客户端通常应递增。
充值以全局 transactionId 幂等：同一用户、金额、类型重试返回原流水和当时余额，改变交易内容返回
transaction_conflict。收藏重复设置目标状态成功；重复预约返回 reservation_conflict，不新增预约；
重复取消已取消预约成功。开始充电先检查所传预约存在且属于本人，有活动订单时返回现有订单；
重复停止已结算订单返回原最终结果及原结算余额，不重复扣费。
管理员电桩控制按会话范围、操作者、requestId、action 和目标重放；不同连接不共享请求号命名空间。

### 消息类型

| 功能 | 请求 / 响应（或推送） | 值 |
|---|---|---|
| 心跳 | Ping / Pong | 1 / 2 |
| 用户登录 | UserLoginRequest / UserLoginResponse | 1001 / 1002 |
| 资料读取 | UserProfileRequest / UserProfileResponse | 1010 / 1011 |
| 资料更新 | UserProfileUpdateRequest / UserProfileUpdateResponse | 1020 / 1021 |
| 退出 | LogoutRequest / LogoutResponse | 1090 / 1091 |
| 充值 | WalletRechargeRequest / WalletRechargeResponse | 1100 / 1101 |
| 流水 | WalletLedgerRequest / WalletLedgerResponse | 1110 / 1111 |
| 站点 | StationListRequest / StationListResponse | 2001 / 2002 |
| 电桩 | PileListRequest / PileListResponse | 2010 / 2011 |
| 收藏 | FavoriteToggleRequest / FavoriteToggleResponse | 2020 / 2021 |
| 预约 | ReservationCreateRequest / ReservationCreateResponse | 3001 / 3002 |
| 开始 | ChargingStartRequest / ChargingStartResponse | 3010 / 3011 |
| 停止 | ChargingStopRequest / ChargingStopResponse | 3020 / 3021 |
| 活动订单 | ActiveOrderRequest / ActiveOrderResponse | 3030 / 3031 |
| 取消预约 | ReservationCancelRequest / ReservationCancelResponse | 3040 / 3041 |
| 管理员登录 | AdminLoginRequest / AdminLoginResponse | 5001 / 5002 |
| 管理命令 | AdminCommandRequest / AdminCommandResponse | 5010 / 5011 |
| 进度、停止 | ChargingProgressPush / ChargingStoppedPush | 8001 / 8002 |
| 告警 | AlarmPush | 8010 |
| 设备状态 | DeviceStatusPush | 8020，保留枚举，当前无生产推送路径 |

既有消息枚举保持不变。Invalid=0 不用于业务请求。

### 错误码与稳定 reason

| statusCode | 名称 |
|---|---|
| 0 | Success |
| 1001 / 1002 / 1003 / 1004 | InvalidPacket / UnsupportedVersion / UnsupportedMessage / InvalidPayload |
| 1101 / 1102 / 1103 / 1104 / 1105 | Unauthorized / Forbidden / SessionExpired / InvalidCredentials / AccountDisabled |
| 1201 / 1202 / 1203 / 1204 / 1205 | ValidationFailed / Conflict / NotFound / DuplicateRequest / RateLimited |
| 2001 / 3001 / 3002 / 9000 | DatabaseError / NetworkUnavailable / RequestTimeout / InternalError |

消息帧无效时可能直接断开并报告协议错误，不保证能生成业务响应。

| reason | 状态码与含义 |
|---|---|
| transaction_conflict | 1202，充值交易号已用于其他用户、金额或类型 |
| user_frozen | 1202，冻结用户不能新预约或开始新订单 |
| insufficient_balance | 1202，预约/开始余额为零，或结算余额不够；也可作为正常自动停止的 stopReason |
| reservation_conflict / reservation_expired | 1202，已有活动预约、预约已使用/取消，或预约过期 |
| order_conflict / order_not_active | 1202，预约时已有活动订单，或停止对象不处于可结算状态 |
| pile_unavailable | 1202，站点离线或电桩不可用于预约/开始 |
| command_conflict | 1202，管理员控制或告警状态变更与当前状态冲突 |
| export_too_large | 1205，CSV 编码后的响应超过传输安全限额，缩小筛选范围 |
| statistics_overflow | 2001，统计累计值超过 qint64 范围 |
| response_too_large | 9000，最终 JSON 响应超过 4 MiB 时返回固定短错误；超大推送不发送并记录失败 |

## 二、用户接口字段

用户登录请求 `{phone}`，管理员登录请求 `{username,password}`。手机号须以 1 开头且共 11 位，
新用户自动注册，余额 0。资料读取请求 `{}`。
用户登录/资料读取另有 created（是否本次新建用户，资料读取恒 false）和 role=user；
管理员登录返回 adminId, username, permissions, role=administrator。

| 接口 | 请求 | 成功负载字段（另含 message） |
|---|---|---|
| 资料读取/更新 | 更新 nickname、avatarBase64 至少有一个非空；允许同时提交 | userId, phone, nickname, avatar, avatarPath, balanceCents, status |
| 充值 | amountCents, transactionId | recordId, transactionId, orderId（无订单为 null）, recordType, amountCents, balanceAfterCents, status, createdAt, balanceCents |
| 流水 | 可选 page, pageSize | items（同充值流水字段，无 balanceCents）, page, pageSize, total |
| 站点 | 可选 region, address, latitude, longitude, radiusKm, sort, page, pageSize | items, page, pageSize, total；站点项见下 |
| 电桩 | stationId，可选 page, pageSize | stationId, items, page, pageSize, total；电桩项见下 |
| 收藏 | stationId, favorited（布尔） | stationId, favorited, updatedAt |
| 预约创建 | stationId，可选 pileId, durationMinutes | reservationId, userId, stationId, pileId, status, reservedAt, expiresAt, usedAt |
| 预约取消 | reservationId | 同预约创建，status=cancelled |
| 活动订单 | {} | active=false；存在时 active=true，订单快照字段直接放在负载中，不嵌套 order |
| 开始 | reservationId；不支持仅传 pileId | 订单快照 |
| 停止 | orderId | 最终订单快照和 balanceCents |

昵称 trim 后 2–20 字符，允许中文汉字、英文字母、数字、空格、下划线、短横线，允许重名。
头像接受原始标准 Base64 或 `data:image/png;base64,...` / `data:image/jpeg;base64,...`，
验证实际 PNG/JPEG 解码和声明 MIME，一张图片解码字节最多 2 MiB，Base64 主体最多 2796204 字符。
不二次压缩；落盘到数据库目录的 avatars 子目录。avatar 与 avatarPath 返回相对资源路径，
不是 Base64 或 HTTP URL，当前没有头像下载接口；新用户为 `default://gray-avatar`，由客户端显示默认头像。
格式错误/超限为 1201；已冻结但会话仍认证的用户可更新资料，资料更新不返回冻结错误。

充值为立即记账，无异步充值状态；金额 100–1000000 cents，transactionId 为非空字符串且最长 128 字符。
新充值时冻结返回 1105，原交易重放仍返回原结果。流水 recordType 为 recharge（正）、
charge_payment（扣费，通常负数，零费用可为 0）、refund（正，持久层支持但无用户退款 API），
当前充值与支付 status=success。流水按 createdAt 倒序并以 id 倒序打破同时间排序，无类型筛选。

站点只列 online；每项为 stationId, name, address, latitude, longitude, priceCentsPerKwh,
totalPiles, availablePiles, favorited, status, distanceKm。坐标必须成对，纬度 -90–90、经度 -180–180；
Haversine 计算半径，radiusKm 默认 10、范围 1–100，距离四舍五入至两位小数。
region/address 分别在站名或地址作包含匹配，有坐标时也同时应用文本筛选。
sort 允许 distance/name/price，默认有坐标按距离、无坐标按 ID；无坐标 distanceKm=null，半径不参与过滤。
无结果成功返回空列表。

电桩项为 stationId, pileId, pileCode, type, powerKw, status, priceCentsPerKwh, updatedAt。
type=fast/slow；status=available/reserved/charging/offline/fault/disabled（数据库 idle 对外为 available）。
不存在站点返回 1203；查询离线站点的电桩仍返回数据。收藏不存在站点返回 1203，离线站点仍可收藏/取消。

预约 durationMinutes 默认 15、允许整数 5–30，服务端生成 expiresAt，不接受客户端指定到期时间。
不指定桩时选择站内空闲桩。一用户最多一个活动预约或充电订单；用户余额必须大于 0。
预约状态 active/used/cancelled/expired。取消他人预约为 1102；已过期为 1202/reservation_expired。
定时任务每 60 秒清理到期预约，创建和开始路径也验证过期，释放桩时不覆盖故障等不可用状态。

订单快照字段为 orderId, orderNo, seq, userId, stationId, pileId, reservationId, status,
startedAt, stoppedAt, durationSec, energyWh, energyKwh, priceCentsPerKwh, payableCents,
feeCents, stopReason, createdAt, updatedAt。payableCents 是 feeCents 的兼容同值字段，
不是待付款标志；没有 pendingPayment 等字段。当前流转为 reserved → charging → 内存 stopping
→ completed/fault_stopped；数据库还保留 cancelled，用户接口不创建这种订单。
活动订单从持久层查询，结算重试期间可能仍返回 charging；不能据此断言设备仍在运行。
停止成功已完成钱包扣款和电桩状态更新，不包含对账中/待付款阶段。

## 三、实时推送与设备边界

每 1 秒触发充电采样任务，数据库/线程忙时可能延后。每个订单只在采样推进时发进度，
同秒或旧采样不重复产生事件。8001 包含完整订单快照及 powerKw，status=charging。
8002 包含最终订单快照及 balanceCents，status=completed/fault_stopped。
二者 header.statusCode=0，requestId=0；异常停止也不是协议失败响应。
seq 对同一订单严格递增并持久化，重启后继续；客户端按 orderId/seq 去重并丢弃旧帧。
服务器按队列顺序发送到订单所属用户的所有在线 session 和所有已认证管理员。
首次结算成功排队一次停止推送，随后 tick 发出；重复停止不再新增停止事件。
没有应用层 ACK 或离线补发保证，入发送队列失败记录 push_records，已结算事务不回滚。
断线后重新登录并查询活动订单；需要已结算历史可由管理员 order.list 查询。

计费为 `floor(energyWh * priceCentsPerKwh / 1000)`，订单建立时固定站点价格，不实现分时电价。
模拟电量依额定功率和总时长计算，避免逐 tick 舍入累积。自动停止不超支。
stopReason 为 user_stop、admin_stop、fully_charged、insufficient_balance、device_fault、
device_offline、over_temperature、over_current、connection_lost。
前四种通常 completed；设备/传感器异常为 fault_stopped，故障桩保留不可用状态。

默认传感器为模拟器：已连接、25°C、400V 下的额定电流、60000Wh 充满阈值。
温度 >60°C、电流超过 powerKw*1000/400、无效传感器值、设备 fault/offline 或连接丢失触发严重告警。
AlarmPush 字段为 alarmId, pileId, orderId, alarmType, severity, message, status, created；
本运行时 alarmType 是五种设备异常 stopReason，severity=critical、status=open、created=true。
同订单同类型未 resolved 前去重。充满/余额不足通过停止推送表达，不创建故障告警。
告警可由管理员 acknowledged/resolved；当前无自动恢复通知。客户端 TCP 断线本身不等于设备连接丢失。
真实硬件命令、传感器接入和设备 ACK 不在当前实现内；重启/启停操作作用于模拟设备持久状态。

## 四、管理员 action

统一 5010/5011，请求所有参数与 action 平级；正常执行路径响应回显原 action。
未知 action 为 1003（不保证回显 action），用户角色调用管理请求为 1102。

| action | 参数 | 返回业务字段 |
|---|---|---|
| dashboard.summary | 无 | total/today/monthRevenueCents, total/today/monthOrderCount, activeOrderCount；同值 revenueMetrics/orderMetrics；pileStatus, userCount, stationCount, openAlarmCount, energyKwh, updatedAt |
| revenue.trend | days 默认 7，1–366 | days, points（也名 items），每项 date, revenueCents, orderCount |
| pile.status.summary | 无 | available, idle（同值）, reserved, charging, fault, offline, disabled, total |
| station.list | keyword, status=online/offline，可选分页 | 管理站点列表 |
| station.detail | stationId | 管理站点项 |
| station.create | name, address, longitude, latitude, priceCentsPerKwh；可选 status | 创建后的管理站点项 |
| station.update | stationId，加至少一个上述维护字段 | 更新后的管理站点项 |
| pile.list | stationId（0/不传表示全部）, status，可选分页 | 管理电桩列表 |
| pile.detail | pileId | 管理电桩项 |
| pile.stop | pileId 或 orderId，同时传时必须对应 | 最终订单快照及 balanceCents |
| pile.restart / pile.enable / pile.disable | pileId | 电桩状态项 |
| user.list | phone 或兼容 phoneKeyword，可选分页 | 管理用户列表，手机号包含匹配 |
| user.freeze / user.unfreeze | userId | 管理用户项 |
| order.list / charging.active.list | status, phone, orderNo, from, to，可选分页；active 强制 charging | 管理订单列表 |
| report.export | 同订单筛选；返回全部命中项，不受分页切片影响 | filename/fileName, content, encoding=UTF-8, mimeType, total |
| alarm.list | status, severity，可选分页 | 管理告警列表 |
| alarm.detail | alarmId | 管理告警项 |
| alarm.handle | alarmId, status=acknowledged/resolved | 处理后的管理告警项 |

管理站点项：stationId, name/stationName, address, longitude, latitude, priceCentsPerKwh, status,
totalPileCount/pileCount, idlePileCount, onlinePileCount, createdAt, updatedAt。
名称 trim 后 1–100 字符，地址 1–500；价格整数 0–1000000。管理电桩项：pileId, stationId,
pileCode, type/chargeType, powerKw, status, totalChargeCount, totalChargeSeconds, lastHeartbeatAt,
updatedAt；查询另含 stationName, priceCentsPerKwh。管理用户项：userId, phone, nickname,
avatar, balanceCents, status, createdAt, updatedAt，用户状态 normal/frozen。
管理订单项为订单快照加 phone, nickname, pileCode, stationName, powerKw, durationSeconds（同 durationSec）。
订单 status 可筛 charging/completed/fault_stopped/cancelled，from/to 按开始日期包含两端，
orderNo 包含匹配。管理告警项：alarmId, pileId, orderId, alarmType, severity, message, status,
occurredAt, recoveredAt, handledByAdminId, pileCode, orderNo；severity=info/warning/critical，
status=open/acknowledged/resolved。已 resolved 不可回退，重复处理为相同目标状态成功。

营收只累计 completed/fault_stopped 的 feeCents，按停止日期统计；订单量按开始日期统计，
含未结算订单；日期以服务端 UTC 为准。CSV 为带 BOM 的 UTF-8 文本，列为
orderNo,phone,stationName,pileCode,status,energyWh,feeCents,startedAt,stoppedAt，
返回内容供客户端保存；具有公式前缀的文本加单引号。过大返回 export_too_large。

兼容既有管理端别名：dashboard.revenue→revenue.trend；stations.list/detail/create/update→station.*；
piles.list/detail→pile.*；users.list→user.list；orders.list→order.list；alarms.list/detail→alarm.*；
charging.stop→pile.stop。users.freeze 另传 frozen 布尔映射冻结/解冻；piles.control 另传
command=stop/restart/enable/disable。以 piles. 开头的旧 action 返回空闲状态 idle，规范 pile.* 返回 available。

重启/停用/启用与活动预约、充电订单互斥，冲突为 command_conflict；停用后 disabled，
启用仅允许 disabled/available 并变为 available；重启将 fault/offline 恢复 available，
available 保持原状，disabled 重启后仍 disabled。冻结限制新预约和新订单，不强制停掉已有订单。
管理员电桩控制记录操作者、目标、订单、requestId、结果和详情；状态更改与控制审计完成在事务中提交。
当前 control_records 覆盖管理员电桩控制尝试；用户开始/停止及系统自动停止通过订单、钱包、告警留痕，
没有逐条新增对应 control_records，不能将其描述为所有控制来源统一审计。

## 五、联调示例

以下是解码后的逻辑 JSON，header 仅展示 messageType/requestId/statusCode，其他二进制头字段由
PacketCodec 生成；不可把整个示例当作 TCP 文本发送。示例 ID、时间、流水号是说明值，应使用实际响应。
标注为“节选”的 payload 省略上文已有定义的其余字段。

### 成功、空数据与业务失败

| 功能 | 请求 payload | 成功响应 payload（节选） | 失败响应 payload 示例及 statusCode |
|---|---|---|---|
| 资料读取 | `{}` | `{"userId":1,"phone":"13800138000","nickname":"用户8000","avatar":"default://gray-avatar","avatarPath":"default://gray-avatar","balanceCents":0,"status":"normal","message":"操作成功"}` | `{"message":"请先登录"}` / 1101 |
| 资料更新 | `{"nickname":"易子恒"}` | `{"userId":1,"nickname":"易子恒","avatar":"default://gray-avatar","balanceCents":10000,"message":"操作成功"}` | `{"message":"请求参数无效"}` / 1201（如 nickname="a"） |
| 充值 | `{"amountCents":10000,"transactionId":"demo-recharge-001"}` | `{"recordId":1,"transactionId":"demo-recharge-001","orderId":null,"recordType":"recharge","amountCents":10000,"balanceAfterCents":10000,"balanceCents":10000,"status":"success","createdAt":"2026-09-06T08:00:00Z","message":"操作成功"}` | `{"message":"交易号已用于其他交易","reason":"transaction_conflict"}` / 1202 |
| 流水 | `{"page":1,"pageSize":20}` | `{"items":[],"page":1,"pageSize":20,"total":0,"message":"操作成功"}` | `{"message":"请求参数无效"}` / 1201（page=0） |
| 站点 | `{"region":"软件园"}` | `{"items":[{"stationId":1,"name":"软件园充电站","distanceKm":null,"availablePiles":2,"favorited":false}],"page":1,"pageSize":20,"total":1,"message":"操作成功"}` | `{"message":"请求参数无效"}` / 1201（仅有 latitude） |
| 电桩 | `{"stationId":1}` | `{"stationId":1,"items":[{"pileId":1,"pileCode":"DL-SP-001","type":"fast","powerKw":60,"status":"available","priceCentsPerKwh":120}],"page":1,"pageSize":20,"total":2,"message":"操作成功"}`（items 仅展示第一项） | `{"message":"充电站不存在"}` / 1203 |
| 收藏 | `{"stationId":1,"favorited":true}` | `{"stationId":1,"favorited":true,"updatedAt":"2026-09-06T08:00:00Z","message":"操作成功"}` | `{"message":"用户或充电站不存在"}` / 1203 |
| 预约 | `{"stationId":1,"pileId":1,"durationMinutes":15}` | `{"reservationId":1,"stationId":1,"pileId":1,"status":"active","expiresAt":"2026-09-06T08:15:00Z","message":"操作成功"}` | `{"message":"当前状态不允许预约操作","reason":"pile_unavailable"}` / 1202 |
| 取消 | `{"reservationId":1}` | `{"reservationId":1,"status":"cancelled","message":"操作成功"}` | `{"message":"当前状态不允许预约操作","reason":"reservation_expired"}` / 1202 |
| 活动订单 | `{}` | `{"active":false,"message":"操作成功"}`，有订单则 `{"active":true,"orderId":1,"status":"charging","feeCents":120,"message":"操作成功"}` | `{"message":"数据处理失败"}` / 2001 |
| 开始 | `{"reservationId":1}` | `{"orderId":1,"reservationId":1,"status":"charging","seq":0,"feeCents":0,"message":"操作成功"}` | `{"message":"当前状态不允许充电操作","reason":"insufficient_balance"}` / 1202 |
| 停止 | `{"orderId":1}` | `{"orderId":1,"status":"completed","feeCents":120,"balanceCents":9880,"stopReason":"user_stop","message":"操作成功"}` | `{"message":"无权停止该订单"}` / 1102 |
| 管理站点 | `{"action":"station.detail","stationId":1}` | `{"action":"station.detail","stationId":1,"name":"软件园充电站","message":"操作成功"}` | `{"action":"station.detail","message":"充电站不存在"}` / 1203 |
| 管理停用 | `{"action":"pile.disable","pileId":2}` | `{"action":"pile.disable","pileId":2,"status":"disabled","message":"操作成功"}` | `{"action":"pile.disable","message":"当前状态不允许执行该操作","reason":"command_conflict"}` / 1202（桩已预约或充电） |
| 管理空列表 | `{"action":"order.list"}` | `{"action":"order.list","items":[],"page":1,"pageSize":20,"total":0,"totalPages":1,"meta":{"page":1,"pageSize":20,"total":0,"totalPages":1},"message":"操作成功"}` | `{"message":"当前账号无权执行该操作"}` / 1102（普通用户） |

站点空结果与流水采用相同分页空格式；详情/变更接口无“空成功对象”，未找到返回 1203。
所有请求均适用未登录 1101、角色不符 1102、在途重复 1204、数据库异常 2001、超时 3002。

管理 action 补充示例：下表成功列均为 payload 节选，正常 statusCode=0，另含原 action 和
message（通常为“操作成功”；alarm.detail/handle 保留告警本身的 message）。
失败列给出触发方式和最终 statusCode/payload。

| 请求 payload | 成功 payload 节选 | 失败示例 |
|---|---|---|
| `{"action":"dashboard.summary"}` | `{"totalRevenueCents":0,"activeOrderCount":0,"stationCount":2,"userCount":0}` | 数据库不可用：2001 / `{"action":"dashboard.summary","message":"数据处理失败"}` |
| `{"action":"revenue.trend","days":1}` | `{"days":1,"points":[{"date":"2026-09-06","revenueCents":0,"orderCount":0}]}` | days=0：1201 / `{"action":"revenue.trend","message":"请求参数无效"}` |
| `{"action":"pile.status.summary"}` | `{"available":3,"idle":3,"reserved":0,"charging":0,"fault":0,"offline":0,"disabled":0,"total":3}` | 数据库不可用：2001 / `{"action":"pile.status.summary","message":"数据处理失败"}` |
| `{"action":"station.list","page":1}` | `{"items":[],"page":1,"pageSize":20,"total":0}`（无匹配时） | page=0：1201 / `{"action":"station.list","message":"请求参数无效"}` |
| `{"action":"station.create","name":"测试站","address":"大连测试路","latitude":38.86,"longitude":121.53,"priceCentsPerKwh":120}` | `{"stationId":3,"name":"测试站","status":"online"}` | 缺 name：1201 / `{"action":"station.create","message":"请求参数无效"}` |
| `{"action":"station.update","stationId":3,"priceCentsPerKwh":130}` | `{"stationId":3,"priceCentsPerKwh":130}` | 不存在 ID：1203 / `{"action":"station.update","message":"充电站不存在"}` |
| `{"action":"pile.list","stationId":1}` | `{"items":[{"pileId":1,"status":"available"}],"total":2}`（items 仅展示第一项） | status="bad"：1201 / `{"action":"pile.list","message":"请求参数无效"}` |
| `{"action":"pile.detail","pileId":1}` | `{"pileId":1,"status":"available","powerKw":60}` | 不存在 ID：1203 / `{"action":"pile.detail","message":"电桩不存在"}` |
| `{"action":"pile.stop","orderId":1}` | `{"orderId":1,"status":"completed","stopReason":"admin_stop","feeCents":120,"balanceCents":9880}` | pileId=2 且无订单：1202 / `{"action":"pile.stop","message":"电桩没有活动订单","reason":"order_not_active"}` |
| `{"action":"pile.restart","pileId":1}` | `{"pileId":1,"status":"available"}`（原 fault/offline/available） | 活动预约：1202 / `{"action":"pile.restart","message":"当前状态不允许执行该操作","reason":"command_conflict"}` |
| `{"action":"pile.enable","pileId":2}` | `{"pileId":2,"status":"available"}`（原 disabled） | 原 fault：1202 / `{"action":"pile.enable","message":"当前状态不允许执行该操作","reason":"command_conflict"}` |
| `{"action":"user.list","phone":"138"}` | `{"items":[{"userId":1,"phone":"13800138000","status":"normal"}],"total":1}` | pageSize=101：1201 / `{"action":"user.list","message":"请求参数无效"}` |
| `{"action":"user.freeze","userId":1}` | `{"userId":1,"status":"frozen"}` | 不存在 ID：1203 / `{"action":"user.freeze","message":"用户不存在"}` |
| `{"action":"user.unfreeze","userId":1}` | `{"userId":1,"status":"normal"}` | userId=0：1201 / `{"action":"user.unfreeze","message":"请求参数无效"}` |
| `{"action":"charging.active.list"}` | `{"items":[{"orderId":1,"status":"charging","feeCents":120}],"total":1}` | from="bad"：1201 / `{"action":"charging.active.list","message":"请求参数无效"}` |
| `{"action":"report.export","from":"2026-09-06","to":"2026-09-06"}` | `{"filename":"orders-20260906-080000.csv","encoding":"UTF-8","total":0,"content":"\ufefforderNo,phone,stationName,pileCode,status,energyWh,feeCents,startedAt,stoppedAt\r\n"}` | 超限：1205 / `{"action":"report.export","message":"导出结果过大，请缩小日期或订单筛选范围","reason":"export_too_large"}` |
| `{"action":"alarm.list","status":"open"}` | `{"items":[],"total":0}` | severity="bad"：1201 / `{"action":"alarm.list","message":"请求参数无效"}` |
| `{"action":"alarm.detail","alarmId":1}` | `{"alarmId":1,"alarmType":"device_fault","status":"open","severity":"critical"}` | 不存在 ID：1203 / `{"action":"alarm.detail","message":"告警不存在"}` |
| `{"action":"alarm.handle","alarmId":1,"status":"resolved"}` | `{"alarmId":1,"status":"resolved","handledByAdminId":1}` | resolved 回退 acknowledged：1202 / `{"action":"alarm.handle","message":"当前状态不允许执行该操作","reason":"command_conflict"}` |

### 完整 envelope 示例与推送

```json
{"header":{"messageType":1100,"requestId":42,"statusCode":0},"payload":{"amountCents":10000,"transactionId":"demo-recharge-001"}}
```

```json
{"header":{"messageType":1101,"requestId":42,"statusCode":0},"payload":{"recordId":1,"transactionId":"demo-recharge-001","orderId":null,"recordType":"recharge","amountCents":10000,"balanceAfterCents":10000,"status":"success","createdAt":"2026-09-06T08:00:00Z","balanceCents":10000,"message":"操作成功"}}
```

停止请求超时示例（不是已经停止的证明）：

```json
{"header":{"messageType":3021,"requestId":46,"statusCode":3002},"payload":{"message":"请求超时，请重试"}}
```

进度和正常停止推送，payload 为节选；示例 60kW、120 cents/kWh 运行 60 秒产生 1kWh、120 cents：

```json
{"header":{"messageType":8001,"requestId":0,"statusCode":0},"payload":{"orderId":1,"seq":60,"status":"charging","energyKwh":1,"powerKw":60,"durationSec":60,"feeCents":120,"updatedAt":"2026-09-06T08:01:00Z"}}
```

```json
{"header":{"messageType":8002,"requestId":0,"statusCode":0},"payload":{"orderId":1,"seq":61,"status":"completed","energyWh":1000,"energyKwh":1,"durationSec":60,"feeCents":120,"payableCents":120,"balanceCents":9880,"stopReason":"user_stop","updatedAt":"2026-09-06T08:01:00Z"}}
```

故障场景替代上述正常停止（不是同一订单第二次结算）；告警先于停止，8001/8002 不携带失败码：

```json
{"header":{"messageType":8010,"requestId":0,"statusCode":0},"payload":{"alarmId":1,"pileId":1,"orderId":1,"alarmType":"device_fault","severity":"critical","message":"充电异常：device_fault","status":"open","created":true}}
```

```json
{"header":{"messageType":8002,"requestId":0,"statusCode":0},"payload":{"orderId":1,"seq":61,"status":"fault_stopped","energyKwh":1,"durationSec":60,"feeCents":120,"balanceCents":9880,"stopReason":"device_fault","updatedAt":"2026-09-06T08:01:00Z"}}
```

## 六、服务端负责人回复

- 确认日期：2026-09-06；接口已注册并有服务/集成测试，测试结果见验收说明。
- 文档位置：本文件、`docs/server-business-acceptance.md`；代码位于 `feat/yiziheng-server-core` 分支。
- 默认联调地址：本机 `127.0.0.1:8888`；未在此文档承诺部署远端服务器。
- 管理员 `admin/123456`；用户用任意合法手机号首次登录生成。站点/电桩种子和建单步骤见验收说明。
- 联调边界：头像下载、真实硬件接入、客户端页面适配需对应模块配合；HTTP 头像地址、支付网关、
  离线推送补发、DeviceStatusPush、自动恢复告警均不是当前协议实现的保证。
