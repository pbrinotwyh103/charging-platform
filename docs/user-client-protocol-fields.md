# 用户端站点功能协议字段

本文档用于与服务端成员联调，字段名称以 JSON payload 为准。所有请求使用非 0 `requestId`，响应沿用同一编号；错误时使用公共 `statusCode` 和 `message`。

## 附近站点查询

请求：`StationListRequest (2001)`

```json
{"region":"浦东新区","address":"人民广场","latitude":31.2304,"longitude":121.4737,"radiusKm":10,"sort":"distance"}
```

响应：`StationListResponse (2002)`

```json
{"items":[{"stationId":12,"name":"示例充电站","address":"上海市...","latitude":31.23,"longitude":121.47,"priceCentsPerKwh":120,"totalPiles":20,"availablePiles":8,"favorited":false}],"serverTime":"2026-09-06T10:00:00Z"}
```

客户端按当前位置计算并稳定排序 `distanceKm`，服务端可返回其参考值但不应要求客户端信任终态数据。

## 站内电桩详情

请求：`PileListRequest (2010)`

```json
{"stationId":12}
```

响应：`PileListResponse (2011)`

```json
{"stationId":12,"items":[{"pileId":101,"pileCode":"A-01","type":"fast","powerKw":120,"status":"available","priceCentsPerKwh":120,"updatedAt":"2026-09-06T10:00:00Z"}]}
```

`status` 建议使用 `available`、`reserved`、`charging`、`offline`、`fault`、`disabled`。

## 收藏切换

建议新增 `FavoriteToggleRequest/Response`：

```json
{"stationId":12,"favorited":true}
```

响应返回 `stationId`、最终 `favorited` 和 `updatedAt`。服务端应保证重复请求幂等。

## 导航参数

站点详情应包含 GCJ-02 `latitude`、`longitude`。客户端导航请求不经过业务服务端，使用配置中的腾讯地图 HTTPS URL；缺少 API key 或地图不可用时保留手动选择和距离排序。

## 地址解析（2030/2031）

`MapGeocodeRequest` 请求字段：`address:string`（去除首尾空白后 1—200 字）、`region?:string`。用户身份来自已认证会话。

`MapGeocodeResponse` 成功字段：`coordinate:{lat:double,lng:double,crs:"GCJ-02"}`、`formattedAddress:string`、`message:string`。失败通过公共 `statusCode` 和 `message` 返回，不回传腾讯地图 Key。

服务端只访问 `https://apis.map.qq.com/ws/geocoder/v1/`，单次调用 3 秒超时，网络错误最多重试一次；无结果、限流和服务不可用分别映射为 `NotFound`、`RateLimited` 和 `NetworkUnavailable`。

## 错误约定

空结果返回成功和空 `items`；网络超时使用 `RequestTimeout (3002)`；站点不存在使用 `NotFound (1203)`；权限问题使用 `Unauthorized (1101)` 或 `Forbidden (1102)`。客户端不得依据网络断开推断预约或订单已结束。
