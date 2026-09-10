# 用户端联调缺口

| 功能 | 请求/响应 | 推荐字段 | 当前状态 |
| --- | --- | --- | --- |
| 资料编辑 | `UserProfileUpdateRequest/Response` | `nickname`, `avatarBase64` | 前端校验与演示完成；消息类型和服务端处理器缺失 |
| 钱包充值 | `WalletRechargeRequest/Response` | `amountCents`, `transactionId` | 前端金额、余额和演示流水完成；服务端业务缺失 |
| 充值记录 | `WalletLedgerRequest/Response` | `items`, `createdAt`, `amountCents`, `balanceCents` | 前端列表完成；服务端查询缺失 |
| 附近站点 | `StationListRequest/Response` | `latitude`, `longitude`, `region`, `items` | 前端搜索、排序与空状态完成；服务端处理器缺失 |
| 电桩详情 | `PileListRequest/Response` | `stationId`, `pileId`, `type`, `powerKw`, `status` | 前端详情与选择完成；消息类型存在，处理器缺失 |
| 收藏 | `FavoriteToggleRequest/Response`、`StationListRequest/Response`（`favoritesOnly=true`） | `stationId`, `favorited`、`items` | 已联通服务端消息处理器；切换操作幂等，收藏列表按当前已认证用户筛选。客户端已覆盖防重、成功确认、失败回滚、加载/空/失败/重试状态。 |
| 预约 | `ReservationCreateRequest/Response` | `stationId`, `pileId`, `expiresAt`, `reservationId` | 前端预约确认完成；消息类型存在，处理器缺失 |
| 活动订单 | `ActiveOrderRequest/Response` | `orderId`, `status` | 前端活动订单页完成；消息类型及服务端业务缺失 |
| 开始/停止充电 | `ChargingStart/StopRequest/Response` | `orderId`, `status`, `message` | 前端充电与结算流程完成；消息类型存在，处理器缺失 |
| 实时快照 | `ChargingProgressPush` | `orderId`, `seq`, `energyKwh`, `powerKw`, `durationSec`, `feeCents`, `updatedAt` | 前端实时/过期/断线状态完成；服务端推送未实现 |
