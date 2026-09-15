# Full Analytics Platform Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement and verify every P0, P1, and P2 requirement in the second-stage analytics requirements document without weakening the first-stage charging workflows.

**Architecture:** Spark jobs publish versioned ODS/DWD/DWS/ADS and model artifacts through an atomic manifest. Flask exposes authenticated, uniformly validated analytics APIs; the Qt server remains the only client-facing business gateway and adds bounded caching and role checks. Web and Qt clients consume versioned results, while an acceptance runner records real evidence instead of treating unexecuted external checks as passed.

**Tech Stack:** Python 3.10, PySpark 3.4.1, Spark MLlib, Flask, Vue, ECharts, C++17, Qt 6, SQLite, Hadoop/HDFS, unittest/QtTest, Playwright-compatible browser checks

**Spec:** `docs/superpowers/specs/2026-09-15-full-analytics-platform-design.md`

## Global Constraints

- Existing first-stage database tables and message numbers remain compatible.
- Qt clients never call Flask directly.
- Analytics failure never blocks station search, reservation, charging, billing, or device control.
- A requirement is complete only when implementation, test, documentation, and actual evidence status agree.
- External Hadoop/Spark/browser checks remain `not_run` until their commands really succeed.
- Generated data, logs, model artifacts, screenshots, and acceptance runs stay outside Git; only sanitized examples are committed.
- New production behavior follows red-green-refactor TDD.

---

### Task 1: Versioned ingestion, quality, and warehouse publication

**Files:**
- Create: `bigdata/pipeline/run_manifest.py`
- Create: `bigdata/pipeline/quality_lineage.py`
- Create: `bigdata/tests/test_pipeline_contracts.py`
- Modify: `bigdata/pipeline/generate_mock_data.py`
- Modify: `bigdata/pipeline/quality_check.py`
- Modify: `bigdata/pipeline/clean_data.py`
- Modify: `bigdata/pipeline/warehouse_etl.py`
- Modify: `bigdata/scripts/run_pipeline.sh`

**Interfaces:**
- Produces: `RunManifest.start(input_paths)`, `publish(layer, outputs)`, `complete()`, and `fail(stage, message)`.
- Produces: versioned manifests containing `batchId`, `dataVersion`, `schemaVersion`, input SHA-256 values, row counts, timestamps, and published paths.

- [ ] **Step 1: Write failing contract tests**

```python
def test_manifest_is_deterministic_and_atomic(tmp_path):
    source = tmp_path / "orders.csv"
    source.write_text("order_id\n1\n", encoding="utf-8")
    run = RunManifest(tmp_path / "warehouse", batch_id="batch-1")
    run.start([source])
    run.publish("ods", [source])
    current = run.complete()
    assert current["batchId"] == "batch-1"
    assert len(current["inputs"][0]["sha256"]) == 64

def test_quality_lineage_counts_downstream_impact():
    report = summarize_lineage([{"ruleId":"ORDER_TIME","entityId":"o1"}],
                               {"o1":["dwd_orders","ads_overview"]})
    assert report[0]["affectedOutputs"] == ["ads_overview", "dwd_orders"]
```

- [ ] **Step 2: Run the tests and verify missing modules fail**

Run: `python3 -m unittest bigdata.tests.test_pipeline_contracts -v`

- [ ] **Step 3: Implement manifests, batch fields, atomic current pointer, and lineage reports**

Use `tempfile.TemporaryDirectory(dir=warehouse_root)`, write `manifest.json`, then `os.replace(staged_manifest, current_manifest)`. Add `batch_id`, `data_version`, and `generated_at` to all persisted layers; output rule-to-table and rule-to-metric impact summaries.

- [ ] **Step 4: Add high-peak, weekday, rain, holiday, station-difference, and unavailable-device generators with a fixed seed**

Each scenario must change only its named factor and emit a scenario manifest so direction tests can compare paired datasets.

- [ ] **Step 5: Run local pipeline tests and idempotency checks**

Run: `python3 -m unittest bigdata.tests.test_pipeline_contracts -v`

Run: `bigdata/scripts/run_pipeline.sh --local --seed 20260915 --batch-id acceptance-a` twice and compare output manifests excluding timestamps.

- [ ] **Step 6: Commit**

```bash
git add bigdata/pipeline bigdata/tests/test_pipeline_contracts.py bigdata/scripts/run_pipeline.sh
git commit -m "feat: publish versioned analytics warehouse runs"
```

### Task 2: Model registry, comparison, uncertainty, and drift

**Files:**
- Create: `bigdata/pipeline/model_registry.py`
- Create: `bigdata/pipeline/model_drift.py`
- Create: `bigdata/pipeline/incremental_features.py`
- Create: `bigdata/tests/test_model_lifecycle.py`
- Modify: `bigdata/pipeline/train_load_model.py`

**Interfaces:**
- Produces: candidates `random_forest`, `linear_regression`, `gradient_boosted_trees` under one evaluator.
- Produces: `model_registry.json`, `drift_report.json`, checkpointed incremental features, and predictions with `lowerBound` and `upperBound`.

- [ ] **Step 1: Write failing lifecycle tests**

```python
def test_selects_lowest_valid_rmse():
    selected = select_candidate([
        {"name":"linear_regression","rmse":5.0,"constraintsPassed":True},
        {"name":"random_forest","rmse":3.0,"constraintsPassed":True}])
    assert selected["name"] == "random_forest"

def test_drift_thresholds_are_stable():
    assert drift_level(0.08) == "normal"
    assert drift_level(0.18) == "attention"
    assert drift_level(0.31) == "severe"
```

- [ ] **Step 2: Verify red**

Run: `python3 -m unittest bigdata.tests.test_model_lifecycle -v`

- [ ] **Step 3: Implement shared time split, evaluation, selection, registry, residual intervals, and 48/72-hour configurable horizons**

Reject candidates with non-finite metrics, negative sessions, or available-pile bounds violations. Estimate intervals from held-out absolute residual quantiles and clamp them to station capacity.

- [ ] **Step 4: Implement feature and error drift plus transactional checkpoints**

Write a checkpoint only after feature outputs and prediction manifest are published. Compare PSI-like binned distribution distance and rolling MAE against the registered baseline.

- [ ] **Step 5: Verify fixed scenario directions and all model tests**

Run: `python3 -m unittest bigdata.tests.test_model_lifecycle -v`

Run: `spark-submit --master local[2] bigdata/pipeline/train_load_model.py --scenario-test`

- [ ] **Step 6: Commit**

```bash
git add bigdata/pipeline bigdata/tests/test_model_lifecycle.py
git commit -m "feat: add analytics model lifecycle management"
```

### Task 3: Uniform authenticated Flask analytics API

**Files:**
- Create: `bigdata/dashboard/api_contract.py`
- Create: `bigdata/dashboard/auth.py`
- Create: `bigdata/dashboard/audit.py`
- Create: `bigdata/tests/test_dashboard_complete_api.py`
- Modify: `bigdata/dashboard/app.py`

**Interfaces:**
- Produces: `{data, meta}` success envelopes and `{error, message, requestId}` errors.
- Produces: overview, revenue, pile status, hourly load, ranking, region, alarms, quality, predictions, metrics, model comparison, drift, scheduling, maintenance, expansion, regulator, tenant, and health endpoints.

- [ ] **Step 1: Add a table-driven failing test for every endpoint and failure state**

```python
def test_remote_requests_require_api_key(client):
    response = client.get("/api/drift", environ_base={"REMOTE_ADDR":"10.0.0.8"})
    assert response.status_code == 401
    assert response.get_json()["error"] == "unauthorized"

def test_tenant_scope_cannot_be_expanded(client, tenant_headers):
    response = client.get("/api/tenant/overview?tenantId=other", headers=tenant_headers)
    assert response.status_code == 403
```

- [ ] **Step 2: Verify red**

Run: `python3 -m unittest bigdata.tests.test_dashboard_complete_api -v`

- [ ] **Step 3: Implement response helpers, limits, constant-time API-key checks, tenant scope, ETag, stale metadata, and sanitized audit records**

Maximum page size is 200, maximum station IDs per request is 100, allowed horizons come from configured `[1,6,24,48,72]`, and no response includes an absolute filesystem path.

- [ ] **Step 4: Verify success, empty, invalid, missing-file, stale, unauthorized, forbidden, and oversized cases**

Run: `python3 -m unittest bigdata.tests.test_dashboard_api bigdata.tests.test_dashboard_complete_api -v`

- [ ] **Step 5: Commit**

```bash
git add bigdata/dashboard bigdata/tests/test_dashboard_complete_api.py
git commit -m "feat: complete authenticated analytics api"
```

### Task 4: Complete web dashboard and visual verification

**Files:**
- Modify: `bigdata/dashboard/templates/index.html`
- Modify: `bigdata/dashboard/static/css/dashboard.css`
- Modify: `bigdata/dashboard/static/js/dashboard.js`
- Create: `bigdata/tests/browser/dashboard.spec.js`
- Create: `bigdata/tests/test_dashboard_assets.py`

**Interfaces:**
- Consumes: Task 3 endpoints.
- Produces: operations, regulator, and tenant views with local assets, filters, automatic refresh, and explicit loading/empty/stale/error states.

- [ ] **Step 1: Write failing asset and browser tests**

```python
def test_dashboard_has_no_public_cdn():
    html = Path("bigdata/dashboard/templates/index.html").read_text("utf-8")
    assert "cdn." not in html and "unpkg.com" not in html
```

Browser assertions cover 1920×1080 and 1366×768, zero horizontal overflow, no console errors, one chart instance per container, and stable heap after 30 refreshes.

- [ ] **Step 2: Verify red**

Run: `python3 -m unittest bigdata.tests.test_dashboard_assets -v`

- [ ] **Step 3: Implement views, local assets, filters, single-flight refresh, chart disposal, state banners, comparison, drift, and advice panels**

- [ ] **Step 4: Generate and inspect visual evidence**

Run: `node bigdata/tests/browser/run-dashboard-tests.js --screenshots artifacts/acceptance/manual/web`

- [ ] **Step 5: Commit**

```bash
git add bigdata/dashboard bigdata/tests
git commit -m "feat: complete analytics dashboard views"
```

### Task 5: Decision-support services and protocol

**Files:**
- Create: `server/services/decisionservice.h`
- Create: `server/services/decisionservice.cpp`
- Modify: `server/services/analyticsservice.*`
- Modify: `server/services/serviceregistry.*`
- Modify: `common/protocol/messagetypes.h`
- Modify: `server/dispatch/messagedispatcher.cpp`
- Modify: `tests/service_test.*`
- Modify: `tests/protocol_test.cpp`
- Modify: `tests/business_integration_test.cpp`

**Interfaces:**
- Produces: model comparison, drift, scheduling, maintenance, expansion, regulator-summary messages.
- Produces stable advice fields: `adviceId`, `kind`, `stationId`, `severity`, `reasons`, `generatedAt`, `modelVersion`, `stale`.

- [ ] **Step 1: Write failing tests for ranking, stable IDs, deduplication, role guards, timeout, malformed JSON, and first-stage non-blocking behavior**

- [ ] **Step 2: Verify red by building protocol and service tests**

Run: `build/bin/protocol_tests && build/bin/service_tests`

- [ ] **Step 3: Implement pure decision rules and extend the bounded analytics adapter**

Scheduling prioritizes severe load with available staff; maintenance prioritizes open severe alarms and unavailable devices; expansion requires sustained utilization and queue pressure. No suggestion executes a control action.

- [ ] **Step 4: Add role and duplicate-request integration rows and run all server tests**

Run: `build/bin/protocol_tests && build/bin/service_tests && build/bin/business_integration_tests`

- [ ] **Step 5: Commit**

```bash
git add common server tests
git commit -m "feat: add analytics decision support protocol"
```

### Task 6: Complete Qt prediction, warning, and advice workflows

**Files:**
- Modify: `user-client/api/clientapi.*`
- Modify: `user-client/pages/homepage.*`
- Modify: `admin-client/controllers/admincontroller.*`
- Modify: `admin-client/pages/overviewpage.*`
- Create: `admin-client/pages/advicepage.h`
- Create: `admin-client/pages/advicepage.cpp`
- Modify: `admin-client/ui/adminmainwindow.*`
- Modify: `tests/user-client/userui_test.*`
- Modify: `tests/admin-client/adminui_test.*`
- Modify: `tests/admin-client/admincontroller_test.cpp`

**Interfaces:**
- User controls: horizon and sort mode; cards show generation time, interval, and recommendation explanation.
- Admin controls: horizon, region, level, stable warning updates, drift and advice views.

- [ ] **Step 1: Write failing UI/controller tests for all controls and states**

```cpp
void UserUiTest::forecastControlsChangeRequestAndExplainPrediction();
void AdminUiTest::warningsFilterAndReplaceStableKeys();
void AdminUiTest::adviceNeverTriggersControlAutomatically();
```

- [ ] **Step 2: Verify red**

Run: `QT_QPA_PLATFORM=offscreen build/bin/user_ui_tests && QT_QPA_PLATFORM=offscreen build/bin/admin_ui_tests`

- [ ] **Step 3: Implement user horizon/sort controls, timestamp/interval rendering, and recommendation reasons**

- [ ] **Step 4: Implement admin filters, warning map keyed by `stationId:horizon:modelVersion`, and read-only advice tabs**

- [ ] **Step 5: Run full Qt client suites and build both applications**

Run: `QT_QPA_PLATFORM=offscreen build/bin/user_ui_tests`

Run: `QT_QPA_PLATFORM=offscreen build/bin/admin_ui_tests && build/bin/admin_controller_tests`

- [ ] **Step 6: Commit**

```bash
git add user-client admin-client tests
git commit -m "feat: complete analytics client workflows"
```

### Task 7: Security, deployment, and complete acceptance evidence

**Files:**
- Create: `scripts/acceptance/run.py`
- Create: `scripts/acceptance/check_environment.py`
- Create: `scripts/acceptance/check_permissions.py`
- Create: `scripts/acceptance/requirements_map.json`
- Create: `scripts/deploy/start_analytics.sh`
- Create: `scripts/deploy/stop_analytics.sh`
- Create: `docs/验收报告模板.md`
- Create: `docs/第二阶段演示说明.md`
- Create: `docs/模型卡.md`
- Modify: `docs/通信协议说明.md`
- Modify: `docs/第二阶段大数据与智能分析项目需求文档.md`
- Modify: `bigdata/README.md`

**Interfaces:**
- Produces: `artifacts/acceptance/<runId>/manifest.json` with each check in `passed`, `failed`, or `not_run` state.
- Produces: a requirements report mapping every requirement ID to implementation, test, evidence, and current status.

- [ ] **Step 1: Write failing runner tests**

```python
def test_unavailable_external_tool_is_not_run_not_passed(tmp_path):
    report = run_check("hdfs", ["missing-hdfs-binary"], tmp_path)
    assert report["status"] == "not_run"

def test_every_requirement_has_traceability():
    matrix = load_requirements_map()
    assert not [row for row in matrix if not row["implementation"] or not row["test"]]
```

- [ ] **Step 2: Verify red**

Run: `python3 -m unittest scripts.acceptance.tests -v`

- [ ] **Step 3: Implement environment, permission, privacy, API, Qt, browser, Spark, HDFS, performance, and restart checks with captured exit codes and sanitized logs**

- [ ] **Step 4: Run all locally available checks**

Run: `python3 scripts/acceptance/run.py --profile local`

- [ ] **Step 5: Run the external profile on the configured course VM**

Run: `python3 scripts/acceptance/run.py --profile course-vm`

Expected: every P0/P1/P2 check is `passed`; any `failed` or `not_run` keeps the corresponding requirement incomplete.

- [ ] **Step 6: Update the requirements document from the generated evidence and verify repository cleanliness**

Run: `python3 scripts/acceptance/run.py --verify-traceability`

Run: `git diff --check && git status --short`

- [ ] **Step 7: Commit**

```bash
git add scripts docs bigdata/README.md
git commit -m "docs: close full analytics acceptance matrix"
```

### Task 8: Final regression and release checkpoint

**Files:**
- Modify only files required by defects found during this task.

**Interfaces:**
- Consumes all prior deliverables.
- Produces a clean, reproducible branch and final evidence manifest.

- [ ] **Step 1: Run Python and pipeline suites**

Run: `python3 -m compileall -q bigdata scripts && python3 -m unittest discover -s bigdata/tests -v`

- [ ] **Step 2: Run all Qt tests**

Run: `bash scripts/run-tests.sh`

- [ ] **Step 3: Build server and both clients from clean shadow directories**

Run: `bash scripts/build.sh`

- [ ] **Step 4: Run complete acceptance profiles and inspect every non-passed result**

Run: `python3 scripts/acceptance/run.py --profile local`

Run: `python3 scripts/acceptance/run.py --profile course-vm`

- [ ] **Step 5: Fix defects using a new failing regression test for each defect, rerun affected suites, and commit each coherent fix**

- [ ] **Step 6: Confirm the final matrix contains no incomplete requirement and commit the release checkpoint**

Run: `python3 scripts/acceptance/run.py --verify-all-passed && git diff --check`

```bash
git add -A
git commit -m "chore: complete stage two analytics acceptance"
```
