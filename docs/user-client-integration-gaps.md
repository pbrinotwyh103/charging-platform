# 用户端联调缺口

| 功能 | 请求/响应 | 推荐字段 | 当前状态 |
| --- | --- | --- | --- |
| 资料编辑 | `UserProfileUpdateRequest/Response` | `nickname`, `avatarBase64` | 消息类型和服务端处理器缺失 |
| 钱包充值 | `WalletRechargeRequest/Response` | `amountCents`, `transactionId` | 服务端业务缺失，客户端仅预留入口 |
| 充值记录 | `WalletLedgerRequest/Response` | `items`, `createdAt`, `amountCents`, `balanceCents` | 服务端业务缺失 |
| 腾讯地址解析 | `MapGeocodeRequest/Response`（2030/2031） | `address`, `region`, `coordinate`, `formattedAddress` | ✅ 已完成服务端腾讯 WebService 适配和客户端状态闭环；部署时需配置 Key |
| 附近站点 | `StationListRequest/Response` | `latitude`, `longitude`, `region`, `items` | ✅ 基础查询已完成；区域、半径和服务端距离排序仍待增强 |
| 电桩详情 | `PileListRequest/Response` | `stationId`, `pileId`, `type`, `powerKw`, `status` | 消息类型存在，处理器缺失 |
| 收藏 | `FavoriteToggleRequest/Response`、`FavoriteListRequest/Response` | `stationId`, `favorited`, `updatedAt`, `items` | ✅ 已完成消息类型、客户端状态、服务端幂等写入和用户隔离查询 |
| 预约 | `ReservationCreateRequest/Response` | `stationId`, `pileId`, `expiresAt`, `reservationId` | 消息类型存在，处理器缺失 |
| 活动订单 | `ActiveOrderRequest/Response` | `orderId`, `status` | 消息类型缺失，服务端业务缺失 |
| 开始/停止充电 | `ChargingStart/StopRequest/Response` | `orderId`, `status`, `message` | 消息类型存在，处理器缺失 |
| 实时快照 | `ChargingProgressPush` | `orderId`, `seq`, `energyKwh`, `powerKw`, `durationSec`, `feeCents`, `updatedAt` | 推送类型存在，服务端推送未实现 |
