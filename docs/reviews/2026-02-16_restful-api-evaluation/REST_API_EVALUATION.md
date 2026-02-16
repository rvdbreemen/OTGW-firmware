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

### v0 Endpoints (Legacy)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v0/otgw/{msgid}` | Get OpenTherm message by ID |
| GET | `/api/v0/devinfo` | Get device information |
| GET | `/api/v0/devtime` | Get device date/time |
| GET/POST | `/api/v0/settings` | Get/update device settings |

### v1 Endpoints (Current)
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

### v2 Endpoints (Optimized)
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v2/otgw/otmonitor` | OpenTherm data (map format) |

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

## Compliance Score Card

| Category | Score | Notes |
|----------|-------|-------|
| HTTP Methods | 7/10 | Good use of GET/POST, missing PATCH/DELETE semantics |
| Status Codes | 6/10 | Missing 201, 202; good use of 400, 405, 413, 414, 500 |
| Resource Naming | 5/10 | Several verb-based names, inconsistent conventions |
| Error Responses | 3/10 | Plain text errors, inconsistent format, HTML 404 |
| Content Negotiation | 5/10 | JSON default is good, no Accept header handling |
| CORS | 6/10 | Present but inconsistent |
| Documentation | 8/10 | Good OpenAPI spec exists |
| Versioning | 9/10 | Excellent URL-based versioning per ADR-019 |
| **Overall** | **6.1/10** | Functional but needs RESTful improvements |

## Recommendations

### Priority 1: Consistent JSON Error Responses (v2)
Create a standard error response helper that returns JSON for all error codes. This is the most impactful improvement.

### Priority 2: Expand v2 with RESTful Resource Naming
Add v2 equivalents for all v1 endpoints with proper resource naming.

### Priority 3: Proper Status Codes
Return 202 Accepted for queued operations (commands, autoconfigure).

### Priority 4: Consistent CORS Headers
Ensure all responses (including errors) include CORS headers.

### Priority 5: JSON 404 for API Endpoints  
Replace HTML 404 with JSON 404 for API routes.

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
