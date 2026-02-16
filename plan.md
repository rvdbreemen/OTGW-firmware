# REST API Improvement — Implementation Plan

Tracks all tasks from the [REST API Evaluation](docs/reviews/2026-02-16_restful-api-evaluation/REST_API_EVALUATION.md) (14 findings, score 5.4/10) and [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md).

## Phase 1A: v2 Backend Foundation ✅ DONE

> Implemented in commits `f81d141`..`83bc746`

- [x] **T1** Create `sendApiError()` JSON error helper (Finding 1)
- [x] **T2** JSON 404 for `/api/*` routes in `sendApiNotFound()` (Finding 10)
- [x] **T3** `GET /api/v2/health` — JSON errors (Finding 1)
- [x] **T4** `GET/POST /api/v2/settings` — JSON errors (Finding 1)
- [x] **T5** `GET/POST /api/v2/sensors/labels` — JSON errors (Finding 1)
- [x] **T6** `GET /api/v2/otgw/otmonitor` — already existed
- [x] **T7** `GET /api/v2/otgw/telegraf` — JSON errors (Finding 1)
- [x] **T8** `GET /api/v2/otgw/messages/{id}` — RESTful resource name (Finding 2, 8)
- [x] **T9** `POST /api/v2/otgw/commands` — body-based, 202 Accepted (Finding 2, 6)
- [x] **T10** `POST /api/v2/otgw/discovery` — resource noun, 202 Accepted (Finding 3, 6)
- [x] **T11** v2 backward-compat aliases: `/otgw/id/`, `/otgw/label/`, `/otgw/command/`, `/otgw/autoconfigure`
- [x] **T12** CORS headers on all v2 error responses via `sendApiError()` (Finding 4)
- [x] **T13** Deprecation comments on v0 block in `restAPI.ino`
- [x] **T14** Deprecation comments on unversioned endpoints in `FSexplorer.ino`
- [x] **T15** Deprecation announcement in README
- [x] **T16** OpenAPI spec updated for v2 endpoints
- [x] **T17** ADR-035 created

## Phase 1B: Complete v2 Backend Endpoints ⬅️ THIS PR

> Missing v2 endpoints from the evaluation that have no v2 equivalent yet.

- [ ] **T18** `GET /api/v2/device/info` — v2 equivalent for `v0/devinfo` (Finding 14: missing endpoint)
  - Uses `sendStartJsonMap`/`sendJsonMapEntry` (map format, not array)
  - Same data as `sendDeviceInfo()` but map format
  - Fixes frontend bug: `index.js:347` calls `v1/devinfo` which doesn't exist
- [ ] **T19** `GET /api/v2/device/time` — RESTful name for `v1/devtime` (Finding 8)
  - Routes to existing `sendDeviceTimeV2()` (already map format)
- [ ] **T20** `GET /api/v2/flash/status` — RESTful name for `v1/flashstatus` (Finding 8)
  - Routes to existing `sendFlashStatus()`
- [ ] **T21** `GET /api/v2/pic/flash-status` — RESTful name for `v1/pic/flashstatus` (Finding 8)
  - Routes to existing `sendPICFlashStatus()`
- [ ] **T22** `GET /api/v2/firmware/files` — versioned replacement for `/api/firmwarefilelist` (Finding 11)
  - Routes to existing `apifirmwarefilelist()`
- [ ] **T23** `GET /api/v2/filesystem/files` — versioned replacement for `/api/listfiles` (Finding 11)
  - Routes to existing `apilistfiles()`
- [ ] **T24** Update OpenAPI spec with new v2 endpoints (T18–T23)

## Phase 1C: Frontend Migration (`index.js`)

> Migrate all frontend calls from deprecated v0/unversioned to v1 or v2.
> Prerequisites: T18 (device/info), T22 (firmware/files) must be done first.

- [ ] **T25** Fix bug: `index.js:347` — change `v1/devinfo` to `v0/devinfo` (interim fix for Finding 14)
- [ ] **T26** Migrate `index.js:220` `refreshGatewayMode()` — `v0/devinfo` → `v2/device/info`
  - Update response parsing from array format to map format
- [ ] **T27** Migrate `index.js:2547` `refreshFirmware()` — `v0/devinfo` → `v2/device/info`
  - Update response parsing from array format to map format
- [ ] **T28** Migrate `index.js:2744` `refreshDevInfo()` — `v0/devinfo` → `v2/device/info`
  - Update response parsing from array format to map format
- [ ] **T29** Migrate `index.js:2978` `refreshDeviceInfo()` — `v0/devinfo` → `v2/device/info`
  - Update response parsing from array format to map format
- [ ] **T30** Migrate `index.js:2160` `loadUISettings()` — `v0/settings` → `v2/settings`
  - Response format is same (array) — no parsing change needed
- [ ] **T31** Migrate `index.js:3033` `refreshSettings()` — `v0/settings` → `v2/settings`
- [ ] **T32** Migrate `index.js:3213` `saveSettings()` — `v0/settings` POST → `v2/settings` POST
- [ ] **T33** Migrate `index.js:3414` `applyTheme()` — `v0/settings` → `v2/settings`
- [ ] **T34** Migrate `index.js:2448` `refreshDevTime()` — `v0/devtime` → `v1/devtime`
  - v1 uses map format; update parsing logic
- [ ] **T35** Migrate `index.js:2564` `refreshFirmware()` — `firmwarefilelist` → `v2/firmware/files`
- [ ] **T36** Migrate `index.js:361` `otgwDebug.settings()` — `v0/settings` → `v2/settings`
- [ ] **T37** Migrate `index.js:347` `otgwDebug.info()` — `v0/devinfo` → `v2/device/info` (replaces T25 interim fix)

## Phase 2: Non-API Endpoint Migration (Future — v1.3.0)

> These endpoints live outside `/api/` and use GET for state-changing actions.
> Lower priority — tracked here for completeness.

- [ ] **T38** `POST /api/v2/device/reboot` — replaces `GET /ReBoot` (Finding 12)
- [ ] **T39** `POST /api/v2/device/reset-wireless` — replaces `GET /ResetWireless` (Finding 12)
- [ ] **T40** `POST /api/v2/pic/upgrade` — replaces `GET/POST /pic?action=upgrade` (Finding 12)
- [ ] **T41** `POST /api/v2/pic/refresh` — replaces `GET/POST /pic?action=refresh` (Finding 12)
- [ ] **T42** `DELETE /api/v2/pic/firmware/{name}` — replaces `GET/POST /pic?action=delete` (Finding 12)
- [ ] **T43** `POST /api/v2/filesystem/upload` — replaces `POST /upload` (Finding 12)

## Phase 3: OTA & Advanced (Future — v2.0.0)

- [ ] **T44** `POST /api/v2/firmware/upload` — wraps `POST /update` (Finding 13)
- [ ] **T45** `GET /api/v2/firmware/status` — wraps `GET /status` (Finding 13)
- [ ] **T46** OPTIONS method support for CORS preflight (Finding 9)
- [ ] **T47** Response metadata (`_meta` object) (Low priority)
- [ ] **T48** Pagination for large collections (Low priority)

---

## Current Focus: Phase 1B + 1C

**Implementation order:**
1. T18–T23: Add remaining v2 backend endpoints
2. T24: Update OpenAPI spec
3. T25–T37: Migrate frontend to v2/v1
4. Verify build compiles
5. Code review

**Estimated impact:** ~2 KB additional flash for backend, 0 bytes RAM.
