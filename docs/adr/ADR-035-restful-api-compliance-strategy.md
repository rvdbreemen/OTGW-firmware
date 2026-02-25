# ADR-035: RESTful API Compliance Strategy

**Status:** Accepted  
**Date:** 2026-02-16  
**Supersedes:** None (extends ADR-019)

## Context

The OTGW-firmware REST API has evolved organically over several years across three versions (v0, v1, v2). While the API is functional and well-versioned (per ADR-019), an evaluation against RESTful standards ([standards.rest](https://standards.rest), [restfulapi.net](https://restfulapi.net)) identified several areas where the API deviates from best practices:

1. **Error responses** use plain text (`text/plain`) instead of structured JSON
2. **404 responses** return HTML instead of JSON for API routes
3. **Command endpoints** place the command in the URL path instead of the request body
4. **Status codes** return 200 OK for queued operations instead of 202 Accepted
5. **CORS headers** are inconsistent across endpoints
6. **Resource naming** uses verbs (e.g., `autoconfigure`, `command`) instead of nouns
7. **Content-type** is inconsistent between success and error responses

These issues make client integration more complex than necessary and deviate from the principle of least surprise.

**Constraints:**
- ESP8266 has limited RAM (~40KB usable) — solutions must minimize memory impact
- Backward compatibility is mandatory — existing integrations must not break (ADR-019)
- No authentication required — local network security model (ADR-032)
- HTTP only — no HTTPS/TLS (ADR-003)

## Decision

**Expand the v2 API with RESTful-compliant endpoints and standardized error handling, while keeping v0 and v1 unchanged.**

### Key Changes

1. **Standardized JSON error responses** — A reusable `sendApiError()` function returns consistent JSON for all error codes:
   ```json
   {"error": {"status": 400, "message": "Invalid message ID"}}
   ```

2. **JSON 404 for API routes** — The `sendApiNotFound()` function returns JSON instead of HTML when the request is for an `/api/*` path.

3. **Expanded v2 endpoints** with RESTful resource naming:
   - `/api/v2/otgw/messages/{id}` — GET single OpenTherm message (was `/otgw/id/{id}`)
   - `/api/v2/otgw/commands` — POST command in request body (was `/otgw/command/{cmd}` in URL)
   - `/api/v2/otgw/discovery` — POST trigger MQTT autodiscovery (was `/otgw/autoconfigure`)
   - `/api/v2/health` — GET health status
   - `/api/v2/settings` — GET/POST device settings
   - `/api/v2/sensors/labels` — GET/POST Dallas sensor labels

4. **Proper HTTP status codes**:
   - 202 Accepted for queued commands and async operations
   - 400 Bad Request with descriptive JSON error messages
   - 404 Not Found as JSON for API routes

5. **Consistent CORS headers** — All v2 responses (including errors) include `Access-Control-Allow-Origin: *`.

### What Does NOT Change

- **v0 and v1 endpoints remain identical** — no modifications, no deprecation
- **No HATEOAS** — too complex for embedded device with minimal benefit
- **No content negotiation** — JSON only (sufficient for all use cases)
- **No authentication** — per ADR-032, local network security model
- **No HTTPS** — per ADR-003

## Alternatives Considered

### Alternative 1: Modify v1 Endpoints In-Place
**Pros:**
- Single version to maintain
- Immediate improvement for all clients

**Cons:**
- Breaks existing client integrations (error format changes)
- Violates ADR-019 (backward compatibility)
- Risks disrupting Home Assistant automations

**Why not chosen:** Breaking changes are unacceptable for a home heating control system.

### Alternative 2: Create v3 API with Full RESTful Redesign
**Pros:**
- Clean-slate design
- Could implement HATEOAS, pagination, full content negotiation
- Maximum RESTful compliance

**Cons:**
- Significant code size increase on ESP8266
- More RAM usage for complex response structures
- Higher maintenance burden (4 API versions)
- Over-engineered for embedded IoT use case

**Why not chosen:** The ESP8266 constraints make a full RESTful API impractical. Pragmatic improvements within v2 achieve the most benefit with minimal cost.

### Alternative 3: Do Nothing — Document Current State Only
**Pros:**
- Zero risk
- No code changes
- No testing needed

**Cons:**
- Client developers continue struggling with inconsistent error handling
- API remains harder to integrate than necessary
- Technical debt accumulates

**Why not chosen:** The cost of improvement is low and benefits are tangible.

## Consequences

### Positive
- **Client integration simplified** — Consistent JSON errors enable standard error handling
- **API discovery improved** — RESTful naming makes endpoints more predictable
- **Future-proof** — v2 expansion provides a clean base for future features
- **Zero breaking changes** — All existing integrations continue working
- **Minimal resource impact** — ~3 KB flash, 0 bytes additional RAM

### Negative
- **Code duplication** — v2 endpoint handlers share logic with v1 handlers
  - *Mitigation:* Shared data access, only response formatting differs
- **More endpoints to test** — Additional v2 endpoints increase test surface
  - *Mitigation:* Automated build verification; manual testing via curl
- **Migration required for benefits** — Clients must opt-in to v2
  - *Mitigation:* Frontend (`index.js`) migrates gradually; v1 remains available

### Risks & Mitigation
- **Flash space usage**: ~3 KB additional — well within 4MB budget
- **Complexity growth**: More code paths — mitigated by shared backend
- **v2 API surface grows**: More endpoints to maintain — acceptable per ADR-019 policy

## Implementation Details

### Error Response Helper
```cpp
// PROGMEM-safe error response with CORS
void sendApiError(int httpCode, const char* message);
void sendApiError(int httpCode, const __FlashStringHelper* message);
```

### v2 Endpoint Registration
```cpp
// v2 API routing in processAPI()
else if (wc > 2 && strcmp_P(words[2], PSTR("v2")) == 0)
{
  // Health
  if (wc > 3 && strcmp_P(words[3], PSTR("health")) == 0) { ... }
  // Settings  
  else if (wc > 3 && strcmp_P(words[3], PSTR("settings")) == 0) { ... }
  // OTGW resources
  else if (wc > 3 && strcmp_P(words[3], PSTR("otgw")) == 0) {
    if (wc > 4 && strcmp_P(words[4], PSTR("messages")) == 0) { ... }
    else if (wc > 4 && strcmp_P(words[4], PSTR("commands")) == 0) { ... }
    else if (wc > 4 && strcmp_P(words[4], PSTR("discovery")) == 0) { ... }
  }
}
```

## Related Decisions
- **ADR-019:** REST API Versioning Strategy (extended, not superseded)
- **ADR-004:** Static Buffer Allocation (error buffers use static allocation)
- **ADR-009:** PROGMEM String Literals (all new strings use PROGMEM)
- **ADR-003:** HTTP Only (no HTTPS considerations)
- **ADR-032:** No Authentication (local network security model)

## References
- Evaluation: `docs/reviews/2026-02-16_restful-api-evaluation/REST_API_EVALUATION.md`
- Improvement Plan: `docs/reviews/2026-02-16_restful-api-evaluation/IMPROVEMENT_PLAN.md`
- RESTful API standards: https://standards.rest
- REST API best practices: https://restfulapi.net
- RFC 7807 (Problem Details): https://tools.ietf.org/html/rfc7807
- Implementation: `src/OTGW-firmware/restAPI.ino`
- OpenAPI spec: `docs/api/openapi.yaml`
