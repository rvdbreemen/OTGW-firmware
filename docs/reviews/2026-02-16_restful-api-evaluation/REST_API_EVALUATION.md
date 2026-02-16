---
# METADATA
Document Title: RESTful API Compliance Evaluation
Review Date: 2026-02-16 09:17:00 UTC
Branch Reviewed: copilot/improve-rest-api-compliance
Target Version: v1.2.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: API Evaluation
Status: COMPLETE
---

# REST API RESTful Compliance Evaluation

## Executive Summary

This document evaluates the OTGW-firmware REST API against RESTful standards as defined by [standards.rest](https://standards.rest) and [restfulapi.net](https://restfulapi.net). The evaluation covers all three API versions (v0, v1, v2) and identifies areas for improvement while respecting the ESP8266 hardware constraints.

**Overall Assessment:** The API is functional and well-structured for an embedded device, but has several areas that deviate from RESTful best practices. A new v2 API expansion is recommended to address these issues while maintaining full backward compatibility.

## Current API Inventory

### v0 Endpoints (Legacy — in `restAPI.ino`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v0/otgw/{msgid}` | Get OpenTherm message by ID |
| GET | `/api/v0/devinfo` | Get device information |
| GET | `/api/v0/devtime` | Get device date/time |
| GET/POST | `/api/v0/settings` | Get/update device settings |

### v1 Endpoints (Current — in `restAPI.ino`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/health` | Device health status |
| GET | `/api/v1/devtime` | Device date/time (map format) |
| GET | `/api/v1/flashstatus` | Unified flash status |
| GET/POST | `/api/v1/settings` | Device settings |
| GET | `/api/v1/pic/flashstatus` | PIC flash status |
| GET | `/api/v1/otgw/telegraf` | OpenTherm data (Telegraf format) |
| GET | `/api/v1/otgw/otmonitor` | OpenTherm data (full) |
| POST | `/api/v1/otgw/autoconfigure` | Trigger MQTT autodiscovery |
| GET | `/api/v1/otgw/id/{msgid}` | OpenTherm message by ID |
| GET | `/api/v1/otgw/label/{msglabel}` | OpenTherm message by label |
| POST/PUT | `/api/v1/otgw/command/{command}` | Send OTGW command |
| GET/POST | `/api/v1/sensors/labels` | Dallas sensor labels |

**Note:** `/api/v1/devinfo` is NOT implemented but is called by the frontend (`index.js:347`). The frontend mostly uses `/api/v0/devinfo` elsewhere.

### v2 Endpoints (RESTful — in `restAPI.ino`)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v2/health` | Device health status |
| GET/POST | `/api/v2/settings` | Device settings |
| GET/POST | `/api/v2/sensors/labels` | Dallas sensor labels |
| GET | `/api/v2/device/info` | Device information (map format) |
| GET | `/api/v2/device/time` | Device date/time (map format) |
| GET | `/api/v2/flash/status` | Unified flash status |
| GET | `/api/v2/pic/flash-status` | PIC flash status |
| GET | `/api/v2/firmware/files` | PIC firmware file listing |
| GET | `/api/v2/filesystem/files` | LittleFS file listing |
| GET | `/api/v2/otgw/otmonitor` | OpenTherm data (map format) |
| GET | `/api/v2/otgw/telegraf` | OpenTherm data (Telegraf format) |
| GET | `/api/v2/otgw/messages/{id}` | OpenTherm message by ID (RESTful name) |
| POST | `/api/v2/otgw/commands` | Send command (body-based, 202 Accepted) |
| POST | `/api/v2/otgw/discovery` | Trigger MQTT autodiscovery (202 Accepted) |

### Unversioned API Endpoints (in `FSexplorer.ino`)
| Method | Endpoint | Description | Source |
|--------|----------|-------------|--------|
| GET | `/api/firmwarefilelist` | List PIC firmware files (JSON array) | `FSexplorer.ino:212` |
| GET | `/api/listfiles` | List filesystem files (JSON array) | `FSexplorer.ino:213` |

### Non-API HTTP Endpoints (various .ino/.h files)
| Method | Endpoint | Description | Source |
|--------|----------|-------------|--------|
| GET/POST | `/pic` | PIC firmware upgrade/refresh/delete | `OTGW-Core.ino:2431` |
| POST | `/upload` | File upload to LittleFS | `FSexplorer.ino:215` |
| GET | `/ReBoot` | Reboot the device | `FSexplorer.ino:216` |
| GET | `/ResetWireless` | Reset WiFi settings and reboot | `FSexplorer.ino:217` |
| GET | `/update` | OTA firmware update page | `OTGW-ModUpdateServer-impl.h:89` |
| POST | `/update` | OTA firmware upload | `OTGW-ModUpdateServer-impl.h:102` |
| GET | `/status` | OTA flash progress status (JSON) | `OTGW-ModUpdateServer-impl.h:95` |
| GET | `/FSexplorer` | Filesystem explorer page (HTML) | `FSexplorer.ino:202` |

### WebSocket Endpoints
| Protocol | Endpoint | Description | Source |
|----------|----------|-------------|--------|
| WS | `ws://{ip}:81` | Real-time OpenTherm message stream | `networkStuff.h` |

## RESTful Compliance Findings

### Finding 1: Inconsistent Error Response Format (HIGH)

**Standard:** Error responses should be structured JSON with consistent format.  
**Reference:** [RFC 7807 - Problem Details for HTTP APIs](https://tools.ietf.org/html/rfc7807)

**Current State:**
```
HTTP/1.1 400 Bad Request
Content-Type: text/plain

400: invalid msgid\r\n
```

**Expected State:**
```json
HTTP/1.1 400 Bad Request
Content-Type: application/json

{"error": {"status": 400, "message": "Invalid message ID"}}
```

**Impact:** Clients must handle both text and JSON error responses, making error handling complex.

**Affected Endpoints:** All endpoints (405, 400, 413, 414, 500 responses)

---

### Finding 2: Command Endpoint Design (MEDIUM)

**Standard:** Use nouns for resources, HTTP methods for actions. Commands should be in the request body, not the URL path.  
**Reference:** [restfulapi.net/resource-naming](https://restfulapi.net/resource-naming/)

**Current State:**
```
POST /api/v1/otgw/command/TT=20.5
```

**Issues:**
1. Command is in the URL path, not the request body
2. "command" is a verb, not a resource noun
3. Returns 200 OK, but the command is queued (not immediately executed)

**Expected State:**
```
POST /api/v2/otgw/commands
Content-Type: application/json

{"command": "TT=20.5"}

HTTP/1.1 202 Accepted
Content-Type: application/json

{"status": "queued", "command": "TT=20.5"}
```

---

### Finding 3: Verb-Based Endpoint Names (MEDIUM)

**Standard:** Use nouns (resources), not verbs (actions) in URIs.  
**Reference:** [restfulapi.net/resource-naming](https://restfulapi.net/resource-naming/)

**Current Verb-Based Endpoints:**
- `/api/v1/otgw/autoconfigure` - should be a resource or action trigger
- `/api/v1/otgw/command/{cmd}` - should use resource noun "commands"

**Assessment:**
- `autoconfigure` is a trigger action, which is acceptable as a POST to a resource endpoint
- The v2 API should model this as: `POST /api/v2/otgw/discovery` (MQTT discovery is a resource concept)

---

### Finding 4: Inconsistent CORS Headers (LOW)

**Standard:** CORS headers should be consistent across all endpoints.

**Current State:** `Access-Control-Allow-Origin: *` is only set on:
- `sendOTGWvalue()` 
- `sendOTGWlabel()`
- `sendApiNotFound()`
- `sendStartJsonObj()`
- `sendStartJsonMap()`
- `sendStartJsonArray()`

Missing on: Direct `httpServer.send()` responses (errors, commands, settings POST)

---

### Finding 5: Mixed Content-Types for Same Resource (LOW)

**Standard:** APIs should return consistent content-types.

**Current State:**
- Success responses: `application/json`
- Error responses: `text/plain` or `text/html`
- Command response: `text/plain` ("OK")

**Expected:** All responses should be `application/json`.

---

### Finding 6: Missing HTTP 202 Accepted for Queued Operations (MEDIUM)

**Standard:** Use 202 Accepted when a request has been accepted for processing but processing is not complete.

**Current State:**
- `POST /api/v1/otgw/command/{cmd}` returns `200 OK` immediately, but the command is queued
- `POST /api/v1/otgw/autoconfigure` returns `200 OK` immediately, but sends MQTT messages asynchronously

**Expected:** Both should return `202 Accepted` since processing happens asynchronously.

---

### Finding 7: POST vs PUT Semantics (LOW)

**Standard:** PUT for idempotent full replacement, POST for non-idempotent creation, PATCH for partial updates.

**Current State:**
- Settings accept both POST and PUT with same behavior
- Both act as partial update (single setting change)

**Assessment:** This is acceptable for an embedded device. The v2 API should prefer POST for settings updates since they are partial updates (or ideally PATCH).

---

### Finding 8: Resource Naming Conventions (MEDIUM)

**Standard:** Use lowercase with hyphens for multi-word resources. Use plural nouns for collections.

**Current Inconsistencies:**
- `devinfo` → should be `device` or `device/info`
- `devtime` → should be `device/time` or `time`
- `flashstatus` → should be `flash/status` or `flash-status`
- `otmonitor` → not a resource name; `opentherm/data` would be more RESTful
- `telegraf` → tool-specific, not a resource name

---

### Finding 9: No OPTIONS Method Support (LOW)

**Standard:** OPTIONS should return allowed methods for an endpoint (CORS preflight).

**Current State:** No OPTIONS handling exists. CORS preflight requests would fail.

**Impact:** Low for local network use. Would matter if accessed from web apps on different origins.

---

### Finding 10: HTML 404 Response for API Endpoints (MEDIUM)

**Standard:** API endpoints should return JSON for all responses, including errors.

**Current State:**
```html
HTTP/1.1 404 Not Found
Content-Type: text/html

<!DOCTYPE HTML><html><head>...
<h1>OTGW firmware</h1>
[/api/v1/invalid] is not a valid
```

**Expected:**
```json
HTTP/1.1 404 Not Found
Content-Type: application/json

{"error": {"status": 404, "message": "Endpoint not found", "path": "/api/v1/invalid"}}
```

---

### Finding 11: Unversioned API Endpoints (MEDIUM)

**Standard:** All API endpoints should be versioned for consistent evolution and deprecation.
**Reference:** [restfulapi.net/versioning](https://restfulapi.net/versioning/)

**Current State:** Two endpoints bypass the versioning system:
- `GET /api/firmwarefilelist` — returns JSON array of PIC firmware files (`FSexplorer.ino:212`)
- `GET /api/listfiles` — returns JSON array of filesystem files (`FSexplorer.ino:213`)

These are registered directly on the httpServer and do not go through `processAPI()`.

**Impact:** Cannot be evolved without breaking changes. No versioning scheme for future improvements.

**Recommendation:** Add v2 versioned equivalents:
- `GET /api/v2/firmware/files` — PIC firmware file listing
- `GET /api/v2/filesystem/files` — LittleFS file listing

---

### Finding 12: Non-API Endpoints Outside `/api/` Namespace (MEDIUM)

**Standard:** API endpoints that return data or trigger actions should live under a unified `/api/` namespace.
**Reference:** [restfulapi.net/resource-naming](https://restfulapi.net/resource-naming/)

**Current State:** Several action/data endpoints live at root path level:
- `GET/POST /pic?action=upgrade&name=...` — PIC firmware upgrade (`OTGW-Core.ino:2431`)
- `POST /upload` — file upload to LittleFS (`FSexplorer.ino:215`)
- `GET /ReBoot` — reboot device (`FSexplorer.ino:216`)
- `GET /ResetWireless` — reset WiFi and reboot (`FSexplorer.ino:217`)

**Issues:**
1. `GET /ReBoot` uses GET for a state-changing action (violates safe method semantics)
2. `GET /ResetWireless` uses GET for a destructive action
3. `/pic` mixes query parameters for action routing (should be distinct endpoints or POST body)
4. None of these return JSON responses — they return HTML redirects

**Recommendation (future phases):** Add v2 API equivalents:
- `POST /api/v2/device/reboot` — reboot (POST for action)
- `POST /api/v2/pic/upgrade` — PIC firmware upgrade
- `POST /api/v2/filesystem/upload` — file upload

---

### Finding 13: OTA Update Endpoints Outside `/api/` Namespace (LOW)

**Standard:** Firmware update endpoints should follow the same patterns as other API endpoints.

**Current State:** The ESP8266HTTPUpdateServer registers:
- `GET /update` — OTA firmware update page (HTML form) (`OTGW-ModUpdateServer-impl.h:89`)
- `POST /update` — OTA firmware upload binary (`OTGW-ModUpdateServer-impl.h:102`)
- `GET /status` — OTA flash progress (JSON) (`OTGW-ModUpdateServer-impl.h:95`)

**Assessment:** These come from a modified ESP8266 library and are tightly coupled to the HTML update workflow. Low priority for RESTful migration since the update process has specific browser requirements.

**Recommendation (future):** Consider `POST /api/v2/firmware/upload` and `GET /api/v2/firmware/status` as RESTful wrappers.

---

### Finding 14: Missing `/api/v1/devinfo` Endpoint (BUG)

**Standard:** Endpoints referenced by the frontend should exist.

**Current State:** The frontend (`index.js:347`) calls `/api/v1/devinfo`, but this endpoint does NOT exist in the v1 routing. Only `/api/v0/devinfo` exists. The call falls through to `sendApiNotFound()`.

**Impact:** The frontend's `otgwDebug.api()` function fails for `v1/devinfo`. Most frontend code correctly uses `v0/devinfo`.

**Recommendation:** Add `/api/v2/device/info` in v2 for the device information endpoint.

---

## Compliance Score Card (Updated after Phase 1 implementation)

| Category | Before | After | Notes |
|----------|--------|-------|-------|
| HTTP Methods | 6/10 | 7/10 | v2 uses proper POST for actions; legacy GET actions remain |
| Status Codes | 6/10 | 8/10 | v2 uses 202 Accepted for queued ops; v1 still returns 200 |
| Resource Naming | 4/10 | 7/10 | v2 uses resource nouns: device/info, device/time, otgw/messages, firmware/files |
| Error Responses | 3/10 | 8/10 | v2 all JSON errors via sendApiError(); v0/v1 still plain text |
| Content Negotiation | 5/10 | 5/10 | JSON default is good, no Accept header handling |
| CORS | 6/10 | 8/10 | v2 consistent CORS on all responses including errors |
| Documentation | 7/10 | 8/10 | OpenAPI spec covers all v2 endpoints |
| Versioning | 7/10 | 9/10 | All endpoints now have v2 equivalents; unversioned have v2 replacements |
| Completeness | 5/10 | 9/10 | v2 covers all API resources; frontend fully migrated to v2 (zero v0/v1 calls) |
| **Overall** | **5.4/10** | **7.7/10** | Significant improvement; remaining gaps are legacy actions and RFC 7231 Allow header |

## Recommendations

### Priority 1: Consistent JSON Error Responses (v2) ✅ DONE
Create a standard error response helper that returns JSON for all error codes. This is the most impactful improvement.

### Priority 2: Expand v2 with RESTful Resource Naming ✅ DONE
Add v2 equivalents for all v1 endpoints with proper resource naming.

### Priority 3: Proper Status Codes ✅ DONE
Return 202 Accepted for queued operations (commands, autoconfigure).

### Priority 4: Consistent CORS Headers ✅ DONE
Ensure all responses (including errors) include CORS headers.

### Priority 5: JSON 404 for API Endpoints ✅ DONE
Replace HTML 404 with JSON 404 for API routes.

### Priority 6: Add v2 Device Info Endpoint (Phase 1) ✅ DONE
Add `/api/v2/device/info` — device information was missing from v1 and v2. Frontend uses it.

### Priority 7: Version Unversioned Endpoints (Phase 1) ✅ DONE
Add versioned equivalents for `/api/firmwarefilelist` and `/api/listfiles`:
- `GET /api/v2/firmware/files` — PIC firmware file listing
- `GET /api/v2/filesystem/files` — filesystem file listing

### Priority 8: Frontend Migration ✅ DONE
All frontend API calls migrated from deprecated v0/unversioned to v2 endpoints.

### Priority 9: RESTful Action Endpoints (Phase 2 — Future)
Add proper POST-based action endpoints to replace GET-based actions:
- `POST /api/v2/device/reboot` — replaces `GET /ReBoot`
- `POST /api/v2/pic/upgrade` — replaces `GET/POST /pic?action=upgrade`
- `POST /api/v2/filesystem/upload` — replaces `POST /upload`

### Priority 9: OTA Update Wrappers (Phase 3)
Consider RESTful wrappers for the OTA update endpoints:
- `POST /api/v2/firmware/upload` — wraps `POST /update`
- `GET /api/v2/firmware/status` — wraps `GET /status`

## Constraints

These recommendations must respect:
- **ADR-001:** ESP8266 platform (limited RAM/flash)
- **ADR-003:** HTTP only (no HTTPS)
- **ADR-004:** Static buffer allocation
- **ADR-009:** PROGMEM for string literals
- **ADR-019:** Backward-compatible API versioning
- **ADR-032:** No authentication (local network only)

## Related Documents

- [ADR-035: RESTful API Compliance Strategy](../../adr/ADR-035-restful-api-compliance-strategy.md)
- [REST API Improvement Plan](IMPROVEMENT_PLAN.md)
- [OpenAPI Specification](../../api/openapi.yaml)
