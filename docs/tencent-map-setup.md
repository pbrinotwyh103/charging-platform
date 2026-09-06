# 腾讯地图 WebService 配置

需求 12 的地址解析按概要设计说明书实现：用户端通过项目 TCP 协议请求服务端，服务端使用 `QNetworkAccessManager` 访问腾讯位置服务 HTTPS 地址解析接口。腾讯地图 Key 不会通过业务响应发送给客户端。

## 开发者需要完成的配置

1. 登录[腾讯位置服务控制台](https://lbs.qq.com/dev/console/application/mine)，创建应用和 Key。
2. 为该 Key 开启 WebService API，并分配地址解析接口额度；部署到固定服务器后建议配置调用来源/IP限制。
3. 使用以下任一方式配置 Key：

   - 推荐：启动服务端前设置环境变量 `TENCENT_MAP_KEY`。
   - 或将 `config/app.ini.example` 复制为不会被 Git 提交的 `config/app.ini`，填写 `[map]` 下的 `tencent_key`。

4. 重启服务端。Key 在服务端初始化时读取，客户端不需要配置 Key。

```bash
export TENCENT_MAP_KEY='你的腾讯位置服务Key'
./build/bin/charging_server
```

也可以使用本地配置：

```ini
[map]
tencent_key=你的腾讯位置服务Key
```

## 验收方法

1. 启动服务端和用户端并完成登录。
2. 在首页选择区域，输入包含城市、区县和门牌号的详细地址。
3. 点击“解析地址并搜索”，确认页面先显示地址解析状态，再显示 GCJ-02 经纬度并查询附近站点。
4. 分别验证空地址、无结果地址、断网、无效 Key 和达到调用额度时的提示。
5. 删除 Key 后仍可使用“模拟定位”查询和按距离排序。

腾讯地址解析接口为 `https://apis.map.qq.com/ws/geocoder/v1/`。程序限定 HTTPS、禁止自动重定向，单次调用 3 秒超时，网络错误最多重试一次。
