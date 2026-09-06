# 用户端—服务端接口确认单

> 用途：请服务端负责人逐项填写并回复。客户端将以本文件确认结果为联调依据。

## 一、公共协议（请先确认）

| 项目 | 客户端要求 | 服务端填写/确认 |
|---|---|---|
| 协议版本 | 沿用现有公共协议版本 |  |
| requestId | 所有请求使用非 0 requestId；响应必须原样返回 |  |
| 幂等 | 重复 requestId 或业务幂等键的处理方式 |  |
| 用户身份 | 用户 ID 从已认证 session 获取，客户端不传可伪造的 userId |  |
| 金额 | 统一使用整数 `cents`，禁止浮点金额 |  |
| 时间 | ISO 8601，明确时区（建议 UTC） |  |
| 坐标 | 明确使用 GCJ-02；站点经纬度字段名固定 |  |
| 分页 | `page`/`pageSize`/`total` 或明确说明不分页 |  |
| 错误 | `statusCode` + `message`；确认各业务错误码 |  |
| 超时 | 请求超时建议使用 `RequestTimeout(3002)` |  |

## 二、需要新增或确认的消息类型

请确认消息类型枚举值，并同步修改 `common/protocol/messagetypes.h`、服务端分发器和客户端 API。

| 功能 | 请求 | 响应/推送 | 消息枚举值（服务端填写） |
|---|---|---|---|
| 资料更新 | `UserProfileUpdateRequest` | `UserProfileUpdateResponse` |  |
| 钱包充值 | `WalletRechargeRequest` | `WalletRechargeResponse` |  |
| 充值记录 | `WalletLedgerRequest` | `WalletLedgerResponse` |  |
| 收藏切换 | `FavoriteToggleRequest` | `FavoriteToggleResponse` |  |
| 活动订单 | `ActiveOrderRequest` | `ActiveOrderResponse` |  |
| 预约取消 | `ReservationCancelRequest` | `ReservationCancelResponse` |  |

现有枚举 `StationList`、`PileList`、`ReservationCreate`、`ChargingStart/Stop`、`ChargingProgressPush`、`ChargingStoppedPush` 也请确认是否保持不变。

## 三、接口字段确认

### 1. 个人资料更新

- 请求：`nickname`、`avatarBase64` 是否允许同时为空？
- 头像：允许的 MIME、最大 Base64 大小、服务端是否再次压缩、返回头像 URL 还是 Base64。
- 昵称：长度、允许字符、重复昵称是否允许。
- 响应：请返回完整用户资料，至少包括 `nickname`、`phone`、`avatar`/`avatarUrl`、`balanceCents`。
- 错误：非法格式、超限、用户被冻结分别使用什么错误码？

服务端填写：

```text
请求字段：
响应字段：
校验规则：
错误码：
```

### 2. 钱包充值与流水

- 充值请求：`amountCents`、客户端幂等键 `transactionId`。
- 响应：充值是否立即成功；返回 `balanceCents`、流水 ID、创建时间和状态。
- 重复提交：相同 `transactionId` 必须返回同一业务结果，不得重复入账。
- 流水查询：是否分页，支持哪些流水类型，金额正负号如何表示。

服务端填写：

```text
最小/最大充值金额：
幂等字段：
充值状态枚举：
流水字段及分页：
```

### 3. 附近站点（现有 2001/2002）

请求建议：`region`、`address`、`latitude`、`longitude`、`radiusKm`、`sort`。

响应每个站点至少包含：`stationId`、`name`、`address`、`latitude`、`longitude`、`priceCentsPerKwh`、`totalPiles`、`availablePiles`、`favorited`。

请确认：半径上限、无坐标时的处理、坐标系、排序是否由服务端完成，以及空结果是否返回成功+空 `items`。

### 4. 电桩详情（现有 2010/2011）

请求：`stationId`。

响应：`stationId`、`pileId`、`pileCode`、`type`（`fast`/`slow`）、`powerKw`、`status`、`priceCentsPerKwh`、`updatedAt`。

请确认状态枚举至少是否包含：`available`、`reserved`、`charging`、`offline`、`fault`、`disabled`。

### 5. 收藏切换

请求：`stationId`、目标状态 `favorited`。

响应：最终状态、`updatedAt`。请确认重复请求是否幂等，以及站点被删除/下线时的错误码。

### 6. 预约

请求：`stationId`、可选 `pileId`、预约时长或 `expiresAt`。

响应：`reservationId`、最终状态、`stationId`、`pileId`、`expiresAt`。

请明确：同一用户重复预约、桩已被占用、用户余额不足、用户被冻结、预约过期分别返回什么错误码；是否提供取消预约接口。

### 7. 活动订单

客户端进入充电页和重连后都会调用该接口。

响应建议包含：`orderId`、`status`、`stationId`、`pileId`、`reservationId`、`startedAt`、`updatedAt`、`payableCents`。

请确认无活动订单时的响应格式，以及一个用户是否可能同时存在多个活动订单。

### 8. 开始/停止充电与结算

- 开始请求：`reservationId` 或 `pileId`，响应返回 `orderId` 和服务端状态。
- 停止请求：`orderId`，响应返回停止结果、最终状态和是否待付款。
- 客户端不根据断线或停止请求超时推断充电已结束。

请确认状态枚举及流转：

```text
已预约 -> 充电中 -> 停止中 -> 对账中 -> 待付款 -> 已完成
                                      \-> 异常结束
```

余额不足、预约冲突、电桩不可用、设备故障、重复开始/停止分别对应哪些错误码？

### 9. 实时充电推送（现有 8001/8002）

`ChargingProgressPush` 至少包含：`orderId`、`seq`、`status`、`energyKwh`、`powerKw`、`durationSec`、`feeCents`、`updatedAt`。

请确认：

- `seq` 是否只在同一 `orderId` 内递增；
- 是否可能重复或乱序；
- 推送间隔；
- `feeCents` 是否为服务端暂估值；
- 停止后是否保证发送 `ChargingStoppedPush`；
- `ChargingStoppedPush` 的最终费用、原因和待付款字段。

## 四、服务端实现与联调验收

请确认以下处理器和服务是否已注册：

- `MessageDispatcher`：上述请求均有处理器；
- `StationService`、`PileService`、`ReservationService`、`ChargingService`、`OrderService`、`BillingService`、`UserService` 不再是空壳；
- 所有请求均有未登录、越权、重复请求和数据库异常处理；
- 关键操作具备事务和幂等保证；
- 提供一组可测试账号、站点、电桩、预约和订单种子数据；
- 提供每个接口的成功、空数据、冲突、超时、设备故障示例报文。

服务端负责人回复：

```text
预计完成日期：
接口文档地址/提交：
测试环境地址和端口：
测试账号/站点数据：
当前阻塞项：
```
