---
# METADATA
Document Title: REST API Improvement Plan
Review Date: 2026-02-16 09:17:00 UTC
Target Version: v1.2.0 / v1.3.0
Reviewer: GitHub Copilot Advanced Agent
Document Type: Improvement Plan
Status: PROPOSED
---

# REST API Improvement Plan

## Overview

This plan outlines a phased approach to improving the OTGW-firmware REST API to comply with RESTful standards while maintaining full backward compatibility per ADR-019.

**Strategy:** All improvements are implemented as new v2 endpoints. Existing v0 and v1 endpoints remain unchanged.

## Phase 1: Foundation (v1.2.0) — This PR

### 1.1 JSON Error Response Helper
Create a reusable error response function that returns consistent JSON errors.

**Implementation:**
```cpp
void sendApiError(int httpCode, const char* message) {
  char jsonBuff[200];
  snprintf_P(jsonBuff, sizeof(jsonBuff), 
    PSTR("{\"error\":{\"status\":%d,\"message\":\"%s\"}}"), 
    httpCode, message);
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.send(httpCode, F("application/json"), jsonBuff);
}
```

**Usage in v2 endpoints:**
- All 4xx and 5xx responses use JSON format
- Consistent `{"error": {"status": N, "message": "..."}}` structure

### 1.2 Expand v2 Endpoints
Add RESTful v2 endpoints for all key resources:

| Current Endpoint | v2 Endpoint | Changes |
|------------|------------|---------|
| `/api/v1/health` | `/api/v2/health` | JSON errors |
| `/api/v1/settings` | `/api/v2/settings` | JSON errors |
| `/api/v0/devinfo` | `/api/v2/device/info` | RESTful naming, was missing in v1 |
| `/api/v1/devtime` | `/api/v2/device/time` | RESTful naming |
| `/api/v1/flashstatus` | `/api/v2/flash/status` | RESTful naming |
| `/api/v1/pic/flashstatus` | `/api/v2/pic/flash/status` | RESTful naming |
| `/api/v1/otgw/otmonitor` | `/api/v2/otgw/otmonitor` | Already exists |
| `/api/v1/otgw/telegraf` | `/api/v2/otgw/telegraf` | Keep (integration name) |
| `/api/v1/otgw/id/{id}` | `/api/v2/otgw/messages/{id}` | Resource noun |
| `/api/v1/otgw/label/{label}` | `/api/v2/otgw/messages?label={label}` | Query param |
| `/api/v1/otgw/command/{cmd}` | `/api/v2/otgw/commands` | Body-based, 202 status |
| `/api/v1/otgw/autoconfigure` | `/api/v2/otgw/discovery` | Resource noun, 202 status |
| `/api/v1/sensors/labels` | `/api/v2/sensors/labels` | JSON errors |
| `/api/firmwarefilelist` | `/api/v2/firmware/files` | Versioned, RESTful naming |
| `/api/listfiles` | `/api/v2/filesystem/files` | Versioned, RESTful naming |

### 1.3 JSON 404 for API Routes
Replace HTML 404 response with JSON for `/api/*` routes.

### 1.4 Consistent CORS Headers
Add CORS headers to all v2 error responses via the `sendApiError()` helper.

### 1.5 Frontend Migration Plan (`index.js`)
Upgrade all frontend REST API calls from deprecated v0/unversioned endpoints to v1 or v2.

**Prerequisites:** v2 backend endpoints from 1.2 must be deployed first.

#### Calls using deprecated v0 endpoints (MUST migrate)

| Line(s) | Function | Current Call | Target Call | Notes |
|---------|----------|-------------|-------------|-------|
| 220 | `refreshGatewayMode()` | `v0/devinfo` | `v2/settings` or `v2/device/info` | Reads `gatewaymode` from devinfo |
| 347 | `otgwDebug.info()` | `v1/devinfo` | `v0/devinfo` (fix) or `v2/device/info` | **BUG**: v1/devinfo doesn't exist; falls to 404 |
| 361 | `otgwDebug.settings()` | `v0/settings` | `v2/settings` | Debug console helper |
| 2160 | `loadUISettings()` | `v0/settings` | `v2/settings` | Loads theme/UI prefs on page load |
| 2448 | `refreshDevTime()` | `v0/devtime` | `v1/devtime` or `v2/device/time` | Clock display; note v0→v1 response format differs (array→map) |
| 2547 | `refreshFirmware()` | `v0/devinfo` | `v2/device/info` | Reads PIC info for flash tab |
| 2744 | `refreshDevInfo()` | `v0/devinfo` | `v2/device/info` | Info tab device data |
| 2978 | `refreshDeviceInfo()` | `v0/devinfo` | `v2/device/info` | Settings tab device data |
| 3033 | `refreshSettings()` | `v0/settings` | `v2/settings` | Settings tab |
| 3213 | `saveSettings()` | `v0/settings` (POST) | `v2/settings` (POST) | Settings save |
| 3414 | `applyTheme()` | `v0/settings` | `v2/settings` | Theme loading |

#### Calls using unversioned endpoints (MUST migrate)

| Line(s) | Function | Current Call | Target Call | Notes |
|---------|----------|-------------|-------------|-------|
| 2543, 2564 | `refreshFirmware()` | `firmwarefilelist` | `v2/firmware/files` | PIC firmware list (requires new v2 backend) |

#### Calls using v1 endpoints (keep or optionally upgrade to v2)

| Line(s) | Function | Current Call | Target Call | Notes |
|---------|----------|-------------|-------------|-------|
| 78 | `fetchDallasLabels()` | `v1/sensors/labels` | `v2/sensors/labels` | Optional: v2 has JSON errors |
| 499 | `otgwDebug.health()` | `v1/health` | `v2/health` | Optional: v2 has JSON errors |
| 506 | `otgwDebug.sendCmd()` | `v1/otgw/command/` (POST) | `v2/otgw/commands` (POST body) | Optional: v2 uses body-based command, 202 |
| 3499 | `pollFlashStatus()` | `v1/flashstatus` | keep v1 | No v2 equivalent yet |
| 4309 | `saveDallasLabel()` | `v1/sensors/labels` | `v2/sensors/labels` | Optional: v2 has JSON errors |

#### Calls already on v2 (no action needed)

| Line(s) | Function | Current Call | Notes |
|---------|----------|-------------|-------|
| 2790 | `refreshOTmonitor()` | `v2/otgw/otmonitor` | Already on latest |

#### Migration Notes

- **Response format change for `devtime`:** v0 returns JSON array (`[{"name":"dateTime","value":"..."},...]`), v1 returns JSON map (`{"devtime":{"dateTime":"...",...}}`). Frontend parsing logic must be updated when migrating.
- **Response format change for `devinfo`:** v0 returns array format. If migrating to v2, need to handle map format.
- **Command endpoint change:** v2 `/otgw/commands` expects `{"command":"TT=20.5"}` in POST body instead of URL path. `otgwDebug.sendCmd()` needs updated fetch call.
- **Backend prerequisite:** `/api/v2/device/info` must be implemented before `v0/devinfo` calls can migrate (currently only exists in v0).
- **Backend prerequisite:** `/api/v2/firmware/files` must be implemented before `firmwarefilelist` calls can migrate.

## Phase 2: Non-API Endpoint Migration & Enhanced Responses (v1.3.0) — Future

### 2.1 Migrate Non-API Action Endpoints to v2
Move state-changing actions from root-level GET to proper POST under `/api/v2/`:

| Current Endpoint | v2 Endpoint | Changes |
|------------------|-------------|---------|
| `GET /ReBoot` | `POST /api/v2/device/reboot` | POST for actions, JSON response, 202 Accepted |
| `GET /ResetWireless` | `POST /api/v2/device/reset-wireless` | POST for destructive actions, 202 Accepted |
| `GET/POST /pic?action=upgrade` | `POST /api/v2/pic/upgrade` | Body-based params, JSON response |
| `GET/POST /pic?action=refresh` | `POST /api/v2/pic/refresh` | Body-based params, JSON response |
| `GET/POST /pic?action=delete` | `DELETE /api/v2/pic/firmware/{name}` | Proper HTTP DELETE method |
| `POST /upload` | `POST /api/v2/filesystem/upload` | Under API namespace |

### 2.2 OTA Update API Wrappers
| Current Endpoint | v2 Endpoint | Changes |
|------------------|-------------|---------|
| `POST /update` | `POST /api/v2/firmware/upload` | Under API namespace, JSON status |
| `GET /status` | `GET /api/v2/firmware/status` | Under API namespace |

### 2.3 Response Metadata
Add optional metadata to v2 responses:
```json
{
  "data": { ... },
  "_meta": {
    "timestamp": 1739302200,
    "version": "v2"
  }
}
```

### 2.2 Content Negotiation
Respect `Accept` header for response format selection (JSON default).

### 2.3 OPTIONS Method Support
Add CORS preflight support for v2 endpoints.

## Phase 3: Advanced Features (v2.0.0) — Future

### 3.1 Pagination for Large Collections
Add pagination for sensor data and OpenTherm message lists.

### 3.2 Filtering and Sorting
Add query parameters for filtering OpenTherm data:
```
GET /api/v2/otgw/messages?type=temperature&sort=value
```

### 3.3 Batch Operations
Support batch command submission:
```
POST /api/v2/otgw/commands/batch
[{"command": "TT=20.5"}, {"command": "SW=55"}]
```

## Implementation Guidelines

### ESP8266 Constraints
- All new code must use PROGMEM (`F()`, `PSTR()`) for string literals
- Error response buffers limited to 200 bytes (static allocation)
- No additional heap allocation for error responses
- Reuse existing JSON helper functions where possible

### Backward Compatibility
- **v0 endpoints:** DEPRECATED — will be removed in v1.3.0. Frontend must migrate before removal.
- **Unversioned `/api/` endpoints:** DEPRECATED — will be removed in v1.3.0. Versioned replacements planned.
- **v1 endpoints:** Stable (per ADR-019). Optional migration to v2 for JSON error responses.
- **v2 endpoints:** New RESTful design — preferred for all new code.
- Frontend (`index.js`) migration plan documented in Phase 1.5 above.

### Testing Strategy
- Build verification (Makefile)
- Manual testing via curl/browser
- Existing frontend still works with v1 endpoints
- New v2 endpoints tested independently

## Estimated Impact

### Flash Memory
- New v2 endpoint handlers: ~2-3 KB additional code
- JSON error helper: ~200 bytes
- PROGMEM strings: ~500 bytes (in flash, not RAM)
- **Total: ~3 KB flash** (well within 4MB flash budget)

### RAM Impact
- No new global variables
- Error responses use stack buffers
- Shared data access (same as v1)
- **Total: 0 bytes additional RAM**

## Related Documents

- [REST API Evaluation](REST_API_EVALUATION.md)
- [ADR-035: RESTful API Compliance Strategy](../../adr/ADR-035-restful-api-compliance-strategy.md)
- [ADR-019: REST API Versioning Strategy](../../adr/ADR-019-rest-api-versioning-strategy.md)
