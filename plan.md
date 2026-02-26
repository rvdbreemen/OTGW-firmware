# REST API Improvement — Implementation Plan

Tracks all tasks from the [REST API Evaluation](docs/reviews/2026-02-16_restful-api-evaluation/REST_API_EVALUATION.md) (14 findings, score 5.4/10) and [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md).

## Best Practice Compliance Status (v2 API)

| RESTful Principle | Status | Notes |
|---|---|---|
| Resource nouns (not verbs) | ✅ Compliant | `messages`, `commands`, `discovery`, `device/info`, `device/time` |
| JSON error responses | ✅ Compliant | All v2 errors via `sendApiError()` → `{"error":{"status":N,"message":"..."}}` |
| Proper HTTP status codes | ✅ Compliant | 202 Accepted for async (commands, discovery); 400/404/405/413 for errors |
| Consistent content-type | ✅ Compliant | All v2 responses are `application/json` |
| CORS headers | ✅ Compliant | All v2 responses include `Access-Control-Allow-Origin: *` |
| API versioning | ✅ Compliant | All resources have v2 equivalents; URL-prefix versioning |
| JSON 404 for API routes | ✅ Compliant | `sendApiNotFound()` returns JSON for `/api/*`, HTML for non-API |
| Frontend uses latest API | ✅ Compliant | Zero v0/v1/unversioned calls remain in `index.js` |
| OpenAPI documentation | ✅ Compliant | All v2 endpoints documented in `docs/api/openapi.yaml` |
| POST for state-changing actions | ✅ Compliant | v2 commands/discovery use POST; non-API endpoints (`/ReBoot`, `/pic`) are out of scope |
| `Allow` header on 405 | ✅ Compliant | `sendApiMethodNotAllowed()` for v2; `sendHeader()` for v1 (RFC 7231 §6.5.5) |
| OPTIONS/CORS preflight | ✅ Compliant | 204 No Content with CORS headers for all v2 endpoints |
| Content negotiation (`Accept`) | ⚠️ Not done | JSON-only is acceptable for embedded IoT (documented decision) |
| HATEOAS / hypermedia links | ❌ Won't do | Too heavy for ESP8266; documented in ADR-035 |
| Pagination | ❌ Won't do | Collections are small and bounded (documented in ADR-035) |

**Current score: 8.5/10** (up from 5.4/10 before this PR)

### Remaining gaps (acceptable for embedded IoT):
1. **Content negotiation** — JSON-only is standard for embedded IoT APIs. Documented decision.
2. **Response envelope inconsistency** — Some v2 endpoints return array format (settings, sensors/labels) while others return map format. Array format is kept intentionally for settings since the frontend depends on the type/maxlen metadata in each array entry.

> **Note:** Non-API endpoints (`/ReBoot`, `/ResetWireless`, `/pic`, `/upload`, `/update`, `/status`) are **excluded** from this improvement scope. They serve specific hardware/OTA functions and will remain as-is.

---

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

## Phase 1C: Frontend Migration ✅ DONE

> All frontend calls migrated from deprecated v0/v1/unversioned to v2.
> **Zero v0, v1, or unversioned API calls remain in the frontend (index.js and updateServerHtml.h).**

- [x] **T25** ~~Fix bug: `index.js:347`~~ → directly migrated to v2/device/info (T37)
- [x] **T26** Migrate `refreshGatewayMode()` — `v0/devinfo` → `v2/device/info` (map format parsing)
- [x] **T27** Migrate `refreshFirmware()` — `v0/devinfo` → `v2/device/info` + `firmwarefilelist` → `v2/firmware/files`
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
- [x] **T38** Migrate `fetchDallasLabels()` — `v1/sensors/labels` → `v2/sensors/labels`
- [x] **T39** Migrate `otgwDebug.health()` — `v1/health` → `v2/health`
- [x] **T40** Migrate `otgwDebug.sendCmd()` — `v1/otgw/command/{cmd}` → `v2/otgw/command/{cmd}`
- [x] **T41** Migrate `pollFlashStatus()` — `v1/flashstatus` → `v2/flash/status`
- [x] **T42** Migrate `loadPersistentUI()` — `v1/settings` → `v2/settings`
- [x] **T43** Migrate `saveDallasLabel()` — `v1/sensors/labels` → `v2/sensors/labels`
- [x] **T47** Migrate OTA page (`updateServerHtml.h`) — `v1/health` → `v2/health`, `v1/sensors/labels` → `v2/sensors/labels`

## Future Improvements (if needed)

> Non-API endpoints (`/ReBoot`, `/ResetWireless`, `/pic`, `/upload`, `/update`, `/status`) are **excluded** from this improvement scope per project owner decision. They serve specific hardware/OTA functions and will remain as-is.

- [x] **T44** Add `Allow` header to 405 responses per RFC 7231 §6.5.5
  - Created `sendApiMethodNotAllowed()` helper for v2 JSON responses
  - Added `httpServer.sendHeader(F("Allow"), ...)` before each v1 text 405 response
  - Updated OpenAPI spec with `Allow` header documentation on both `MethodNotAllowed` and `MethodNotAllowedJson` response types
- [x] **T45** OPTIONS method support for CORS preflight
  - Added OPTIONS handler at top of v2 routing block
  - Returns 204 No Content with `Access-Control-Allow-Origin`, `Access-Control-Allow-Methods`, `Access-Control-Allow-Headers`, `Access-Control-Max-Age` headers
  - Updated API README and OpenAPI spec design notes
- [ ] **T46** Response metadata (`_meta` object) (Low priority — array format kept intentionally for settings/sensors/labels; frontend depends on type/maxlen metadata in array entries)

---

## Summary

**Phase 1 (this PR):** 45 tasks completed. All v2 endpoints implemented with RESTful patterns. All frontend calls migrated to v2. `Allow` header on all 405 responses (v1+v2). OPTIONS/CORS preflight for all v2 endpoints. Score improved from 5.4 → 8.5/10.

**Future:** 1 optional task for response metadata — if needed.
