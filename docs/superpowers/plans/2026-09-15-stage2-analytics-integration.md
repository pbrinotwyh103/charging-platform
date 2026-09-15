# Stage Two Analytics Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the existing Flask and Spark analytics outputs to the Qt server, user client, and administrator client with validated prediction queries, low-congestion recommendations, load warnings, caching, and graceful degradation.

**Architecture:** Flask remains a read-only ADS presentation service. A focused Qt `AnalyticsService` calls Flask with bounded synchronous requests from the dispatcher's worker pool, validates and normalizes responses, caches the last successful payload, and exposes role-specific TCP messages. The user client requests recommendations for the station IDs already returned by the live station service; the administrator client requests warnings and renders them on the overview page.

**Tech Stack:** C++17, Qt 6 Core Network Widgets Test, TCP JSON protocol, Python 3, Flask, pytest

**Spec:** `docs/第二阶段大数据与智能分析项目需求文档.md`

## Global Constraints

- The Qt server remains the only client-facing business entry point; Qt clients do not call Flask directly.
- Analytics failures must not block station search, charging, billing, or device-control flows.
- Prediction horizons are exactly 1, 6, or 24 hours.
- Money fields keep the existing integer-cent convention.
- All new TCP requests use nonzero `requestId`, preserve it in responses, and enforce session roles.
- The analytics base URL is configurable and must not depend on a fixed virtual-machine IP.
- Generated `bigdata/data` and `bigdata/logs` directories remain outside Git.

---

### Task 1: Stabilize the Flask prediction contract

**Files:**
- Modify: `bigdata/dashboard/app.py`
- Create: `bigdata/tests/test_dashboard_api.py`

**Interfaces:**
- Consumes: ADS `predictions.json` and `model_metrics.json`.
- Produces: `GET /api/predictions?horizonHours=1|6|24&stationId=<id>` and a sanitized `GET /api/health` response.

- [ ] **Step 1: Write failing Flask API tests**

```python
def test_predictions_filters_horizon_and_station(client, ads_root):
    write_json(ads_root / "predictions.json", [
        {"station_id": 1, "horizon_hours": 1},
        {"station_id": 2, "horizon_hours": 6},
    ])
    response = client.get("/api/predictions?horizonHours=1&stationId=1")
    assert response.status_code == 200
    assert response.get_json() == [{"stationId": 1, "horizonHours": 1}]

def test_predictions_rejects_unsupported_horizon(client):
    response = client.get("/api/predictions?horizonHours=2")
    assert response.status_code == 400
    assert response.get_json()["error"] == "invalid_horizon"
```

- [ ] **Step 2: Run tests and verify they fail because filtering and validation do not exist**

Run: `python3 -m pytest bigdata/tests/test_dashboard_api.py -q`

Expected: filtering assertion fails and unsupported horizon returns 200 instead of 400.

- [ ] **Step 3: Implement query validation, field normalization, and safe health output**

```python
def normalized_prediction(row):
    return {
        "stationId": int(row.get("stationId", row.get("station_id"))),
        "horizonHours": int(row.get("horizonHours", row.get("horizon_hours"))),
        "predictedSessions": float(row.get("predictedSessions", row.get("prediction", 0))),
        "predictedAvailablePiles": max(0, int(row.get("predictedAvailablePiles", row.get("predicted_available_piles", 0)))),
        "generatedAt": row.get("generatedAt", row.get("generated_at", "")),
        "modelVersion": row.get("modelVersion", row.get("model_version", "unknown")),
    }
```

The health response contains service status, ADS availability, and generation time, but not the absolute server path.

- [ ] **Step 4: Run the Flask API tests and verify they pass**

Run: `python3 -m pytest bigdata/tests/test_dashboard_api.py -q`

- [ ] **Step 5: Commit the Flask contract**

```bash
git add bigdata/dashboard/app.py bigdata/tests/test_dashboard_api.py
git commit -m "feat: stabilize analytics prediction api"
```

### Task 2: Add a tested Qt analytics adapter

**Files:**
- Create: `server/services/analyticsservice.h`
- Create: `server/services/analyticsservice.cpp`
- Modify: `server/services/serviceregistry.h`
- Modify: `server/services/serviceregistry.cpp`
- Modify: `server/server.pro`
- Modify: `tests/service-tests.pro`
- Modify: `tests/service_test.h`
- Modify: `tests/service_test.cpp`

**Interfaces:**
- Consumes: Flask JSON endpoints through `AnalyticsService::Fetcher`.
- Produces: `ServiceResult predictions(const QJsonObject &)`, `recommendations(const QJsonObject &)`, `warnings(const QJsonObject &)`, and `status()`.

- [ ] **Step 1: Write failing service tests for validation, normalization, ranking, warning levels, cache fallback, and transport failure**

```cpp
void ServiceTest::analyticsRecommendationsAndFallback()
{
    AnalyticsService service;
    service.setFetcherForTests([](const QUrl &) {
        return AnalyticsFetchResult{true, 200, R"([{"stationId":1,"horizonHours":1,"predictedSessions":9,"predictedAvailablePiles":1}])", {}};
    });
    const auto first = service.recommendations({{"horizonHours", 1}, {"stationIds", QJsonArray{1}}});
    QVERIFY(first.succeeded());
    QCOMPARE(first.payload.value("items").toArray().size(), 1);
    service.setFetcherForTests([](const QUrl &) { return AnalyticsFetchResult{false, 0, {}, "offline"}; });
    const auto cached = service.recommendations({{"horizonHours", 1}, {"stationIds", QJsonArray{1}}});
    QVERIFY(cached.succeeded());
    QVERIFY(cached.payload.value("stale").toBool());
}
```

- [ ] **Step 2: Run the focused service test and verify it fails because `AnalyticsService` does not exist**

Run: `qmake6 tests/service-tests.pro -o build/Makefile.service-tests && make -C build -f Makefile.service-tests -j2 && build/bin/service_tests analyticsRecommendationsAndFallback`

- [ ] **Step 3: Implement the minimal adapter**

```cpp
struct AnalyticsFetchResult {
    bool transportOk = false;
    int statusCode = 0;
    QByteArray body;
    QString error;
};

class AnalyticsService final {
public:
    using Fetcher = std::function<AnalyticsFetchResult(const QUrl &)>;
    ServiceResult predictions(const QJsonObject &payload);
    ServiceResult recommendations(const QJsonObject &payload);
    ServiceResult warnings(const QJsonObject &payload);
    ServiceResult status();
    void setFetcherForTests(Fetcher fetcher);
};
```

Use `CHARGING_ANALYTICS_URL`, defaulting to `http://127.0.0.1:5000`; enforce a two-second timeout; accept only arrays of objects with valid station IDs and matching horizons; sort recommendations by available piles descending then sessions ascending; derive warning levels from expected occupancy; cache each successful query key and mark fallback payloads with `stale: true`.

- [ ] **Step 4: Run focused and full service tests**

Run: `build/bin/service_tests analyticsRecommendationsAndFallback analyticsValidationAndWarnings`

Run: `build/bin/service_tests`

- [ ] **Step 5: Commit the adapter**

```bash
git add server/services/analyticsservice.* server/services/serviceregistry.* server/server.pro tests/service-tests.pro tests/service_test.*
git commit -m "feat: add analytics service adapter"
```

### Task 3: Extend the authenticated TCP protocol

**Files:**
- Modify: `common/protocol/messagetypes.h`
- Modify: `server/dispatch/messagedispatcher.cpp`
- Modify: `tests/protocol_test.cpp`
- Modify: `tests/business_integration_test.h`
- Modify: `tests/business_integration_test.cpp`

**Interfaces:**
- Consumes: `AnalyticsService` methods from Task 2.
- Produces: request and response pairs `LoadPrediction` 4100/4101, `StationRecommendation` 4110/4111, `LoadWarning` 5100/5101, and `AnalyticsStatus` 5110/5111.

- [ ] **Step 1: Write failing protocol and dispatcher tests**

```cpp
QCOMPARE(static_cast<quint16>(Charging::MessageType::LoadPredictionRequest), quint16(4100));
QCOMPARE(static_cast<quint16>(Charging::MessageType::StationRecommendationResponse), quint16(4111));
QCOMPARE(static_cast<quint16>(Charging::MessageType::LoadWarningRequest), quint16(5100));
QCOMPARE(static_cast<quint16>(Charging::MessageType::AnalyticsStatusResponse), quint16(5111));
```

Add unauthorized and role-guard rows showing that prediction and recommendation require a user session, while warning and analytics status require an administrator session.

- [ ] **Step 2: Run protocol and business integration tests and verify the new enum references fail to compile**

Run: `qmake6 tests/protocol-tests.pro -o build/Makefile.protocol-tests && make -C build -f Makefile.protocol-tests -j2`

- [ ] **Step 3: Add message values, response mapping, role mapping, and dispatch calls**

```cpp
case MessageType::LoadPredictionRequest:
    return services->analytics()->predictions(payload);
case MessageType::StationRecommendationRequest:
    return services->analytics()->recommendations(payload);
case MessageType::LoadWarningRequest:
    return services->analytics()->warnings(payload);
case MessageType::AnalyticsStatusRequest:
    return services->analytics()->status();
```

Use an explicit `requiredRoleFor(MessageType)` helper instead of treating every non-admin-command request as a user request.

- [ ] **Step 4: Run protocol and business integration tests**

Run: `build/bin/protocol_tests`

Run: `build/bin/business_integration_tests unauthorizedResponses roleGuards invalidFields`

- [ ] **Step 5: Commit the protocol integration**

```bash
git add common/protocol/messagetypes.h server/dispatch/messagedispatcher.cpp tests/protocol_test.cpp tests/business_integration_test.*
git commit -m "feat: expose analytics over authenticated protocol"
```

### Task 4: Show predictions in the user station list

**Files:**
- Modify: `user-client/api/clientapi.h`
- Modify: `user-client/api/clientapi.cpp`
- Modify: `user-client/pages/homepage.h`
- Modify: `user-client/pages/homepage.cpp`
- Modify: `user-client/ui/usermainwindow.cpp`
- Modify: `tests/user_ui_test.h`
- Modify: `tests/user_ui_test.cpp`

**Interfaces:**
- Consumes: `StationRecommendationResponse` from Task 3.
- Produces: `ClientApi::requestStationRecommendations`, `recommendationsReceived`, and station cards containing forecast horizon, expected free piles, and stale state.

- [ ] **Step 1: Write a failing UI test**

```cpp
void UserUiTest::homePageShowsForecastAndStaleState()
{
    HomePage page;
    page.setStations(QJsonArray{QJsonObject{{"stationId", 1}, {"name", "测试站"}, {"totalPiles", 8}, {"availablePiles", 3}}});
    page.setRecommendations(QJsonObject{{"horizonHours", 1}, {"stale", true},
        {"items", QJsonArray{QJsonObject{{"stationId", 1}, {"predictedAvailablePiles", 5}, {"warningLevel", "normal"}}}}});
    QVERIFY(page.findChild<QListWidget *>("stationList")->item(0)->text().contains("预计空闲 5"));
    QVERIFY(page.findChild<QListWidget *>("stationList")->item(0)->text().contains("缓存预测"));
}
```

- [ ] **Step 2: Build the user UI test and verify it fails because `setRecommendations` does not exist**

Run: `qmake6 tests/user-ui-tests.pro -o build/Makefile.user-ui-tests && make -C build -f Makefile.user-ui-tests -j2`

- [ ] **Step 3: Implement recommendation requests and rendering**

After `StationListResponse`, emit the live stations immediately and request horizon 1 recommendations for their station IDs. Merge forecasts by `stationId`; retain the current station list when analytics fails; label forecasts as predicted and show `缓存预测` when `stale` is true.

- [ ] **Step 4: Run user UI tests**

Run: `QT_QPA_PLATFORM=offscreen build/bin/user_ui_tests`

- [ ] **Step 5: Commit the user integration**

```bash
git add user-client/api/clientapi.* user-client/pages/homepage.* user-client/ui/usermainwindow.cpp tests/user_ui_test.*
git commit -m "feat: show station load recommendations"
```

### Task 5: Show load warnings in the administrator overview

**Files:**
- Modify: `admin-client/controllers/admincontroller.h`
- Modify: `admin-client/controllers/admincontroller.cpp`
- Modify: `admin-client/pages/overviewpage.h`
- Modify: `admin-client/pages/overviewpage.cpp`
- Modify: `admin-client/ui/adminmainwindow.cpp`
- Modify: `tests/admin-client/admincontroller_test.cpp`
- Modify: `tests/admin_ui_test.h`
- Modify: `tests/admin_ui_test.cpp`

**Interfaces:**
- Consumes: `LoadWarningResponse` and `AnalyticsStatusResponse` from Task 3.
- Produces: administrator requests, warning list presentation, forecast timestamp, and stale/unavailable state.

- [ ] **Step 1: Write failing controller and UI tests**

```cpp
void AdminUiTest::overviewShowsLoadWarnings()
{
    OverviewPage page;
    page.setLoadWarnings(QJsonObject{{"horizonHours", 6}, {"items", QJsonArray{
        QJsonObject{{"stationId", 9}, {"stationName", "中心站"}, {"warningLevel", "severe"}, {"predictedAvailablePiles", 0}}
    }}});
    QVERIFY(page.findChild<QLabel *>("loadWarningSummary")->text().contains("中心站"));
    QVERIFY(page.findChild<QLabel *>("loadWarningSummary")->text().contains("严重"));
}
```

- [ ] **Step 2: Build tests and verify the missing request and rendering APIs fail compilation**

Run: `qmake6 tests/admin-controller-tests.pro -o build/Makefile.admin-controller-tests && make -C build -f Makefile.admin-controller-tests -j2`

Run: `qmake6 tests/admin-ui-tests.pro -o build/Makefile.admin-ui-tests && make -C build -f Makefile.admin-ui-tests -j2`

- [ ] **Step 3: Implement controller requests and overview rendering**

Add `requestLoadWarnings(int horizonHours)` and `requestAnalyticsStatus()`. Track their request IDs with the expected response type, emit `loadWarningsReceived` and `analyticsStatusReceived`, and add a compact warning section to `OverviewPage`. Request warnings during overview refresh without blocking the existing SQLite summary, revenue, or pile-status requests.

- [ ] **Step 4: Run administrator tests**

Run: `build/bin/admin_controller_tests`

Run: `QT_QPA_PLATFORM=offscreen build/bin/admin_ui_tests`

- [ ] **Step 5: Commit the administrator integration**

```bash
git add admin-client/controllers/admincontroller.* admin-client/pages/overviewpage.* admin-client/ui/adminmainwindow.cpp tests/admin-client/admincontroller_test.cpp tests/admin_ui_test.*
git commit -m "feat: show predicted load warnings"
```

### Task 6: Verify the complete P0 integration and document operation

**Files:**
- Modify: `bigdata/README.md`
- Modify: `docs/通信协议说明.md`
- Modify: `docs/第二阶段大数据与智能分析项目需求文档.md`

**Interfaces:**
- Consumes: all implemented interfaces from Tasks 1 through 5.
- Produces: reproducible startup configuration, protocol documentation, and updated requirement status.

- [ ] **Step 1: Document exact configuration and requests**

Add `CHARGING_ANALYTICS_URL=http://127.0.0.1:5000`, the four TCP request/response pairs, example payloads, timeout/cache behavior, and a clear note that analytics unavailability does not block first-stage operations.

- [ ] **Step 2: Run Python verification**

Run: `python3 -m compileall bigdata`

Run: `python3 -m pytest bigdata/tests -q`

- [ ] **Step 3: Run the complete Qt regression suite**

Run: `bash scripts/run-tests.sh`

- [ ] **Step 4: Run source and artifact checks**

Run: `git diff --check`

Run: `rg -n "CHARGING_ANALYTICS_URL|LoadPredictionRequest|StationRecommendationRequest|LoadWarningRequest|AnalyticsStatusRequest" bigdata docs common server user-client admin-client tests`

- [ ] **Step 5: Commit documentation and requirement status**

```bash
git add bigdata/README.md docs/通信协议说明.md docs/第二阶段大数据与智能分析项目需求文档.md
git commit -m "docs: document analytics integration"
```
