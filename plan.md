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

## Phase 1B: Complete v2 Backend Endpoints ✅ DONE

> Missing v2 endpoints from the evaluation that have no v2 equivalent yet.

- [x] **T18** `GET /api/v2/device/info` — v2 equivalent for `v0/devinfo` (Finding 14: missing endpoint)
  - New `sendDeviceInfoV2()` function using map format
  - Fixes frontend bug: `index.js:347` called `v1/devinfo` which didn't exist
- [x] **T19** `GET /api/v2/device/time` — RESTful name for `v1/devtime` (Finding 8)
  - Routes to existing `sendDeviceTimeV2()` (already map format)
- [x] **T20** `GET /api/v2/flash/status` — RESTful name for `v1/flashstatus` (Finding 8)
  - Routes to existing `sendFlashStatus()`
- [x] **T21** `GET /api/v2/pic/flash-status` — RESTful name for `v1/pic/flashstatus` (Finding 8)
  - Routes to existing `sendPICFlashStatus()`
- [x] **T22** `GET /api/v2/firmware/files` — versioned replacement for `/api/firmwarefilelist` (Finding 11)
  - Routes to existing `apifirmwarefilelist()`
- [x] **T23** `GET /api/v2/filesystem/files` — versioned replacement for `/api/listfiles` (Finding 11)
  - Routes to existing `apilistfiles()`
- [x] **T24** Update OpenAPI spec with new v2 endpoints (T18–T23)

## Phase 1C: Frontend Migration (`index.js`) ✅ DONE

> All frontend calls migrated from deprecated v0/unversioned to v2.
> Remaining v1 calls (sensors/labels, flashstatus, debug helpers) kept — v1 is stable.

- [x] **T25** ~~Fix bug: `index.js:347`~~ → directly migrated to v2/device/info (T37)
- [x] **T26** Migrate `refreshGatewayMode()` — `v0/devinfo` → `v2/device/info` (map format parsing)
- [x] **T27** Migrate `refreshFirmware()` — `v0/devinfo` → `v2/device/info` (map format parsing)
- [x] **T28** Migrate `refreshDevInfo()` — `v0/devinfo` → `v2/device/info` (map format parsing)
- [x] **T29** Migrate `refreshDeviceInfo()` — `v0/devinfo` → `v2/device/info` (map format parsing)
- [x] **T30** Migrate `loadUISettings()` — `v0/settings` → `v2/settings`
- [x] **T31** Migrate `refreshSettings()` — `v0/settings` → `v2/settings`
- [x] **T32** Migrate `saveSettings()` — `v0/settings` POST → `v2/settings` POST
- [x] **T33** Migrate `applyTheme()` — `v0/settings` → `v2/settings`
- [x] **T34** Migrate `refreshDevTime()` — `v0/devtime` → `v2/device/time` (map format parsing)
- [x] **T35** Migrate `refreshFirmware()` — `firmwarefilelist` → `v2/firmware/files`
- [x] **T36** Migrate `otgwDebug.settings()` — `v0/settings` → `v2/settings`
- [x] **T37** Migrate `otgwDebug.info()` — `v1/devinfo` → `v2/device/info`

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
