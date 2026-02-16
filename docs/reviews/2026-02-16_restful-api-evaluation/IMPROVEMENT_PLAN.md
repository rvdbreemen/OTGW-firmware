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

| v1 Endpoint | v2 Endpoint | Changes |
|------------|------------|---------|
| `/api/v1/health` | `/api/v2/health` | Same (already RESTful) |
| `/api/v1/settings` | `/api/v2/settings` | JSON errors |
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

### 1.3 JSON 404 for API Routes
Replace HTML 404 response with JSON for `/api/*` routes.

### 1.4 Consistent CORS Headers
Add CORS headers to all v2 error responses via the `sendApiError()` helper.

## Phase 2: Enhanced Responses (v1.3.0) — Future

### 2.1 Response Metadata
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
- **v0 endpoints:** Unchanged (legacy support per ADR-019)
- **v1 endpoints:** Unchanged (stable API per ADR-019)
- **v2 endpoints:** New and improved RESTful design
- No existing client code needs modification
- Frontend (`index.js`) can migrate to v2 gradually

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
