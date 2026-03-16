# ADR-050: Centralized API Route Dispatch Table

**Status:** Accepted
**Date:** 2026-03-01
**Relates to:** ADR-035 (RESTful API Compliance), ADR-019 (REST API Versioning)

## Context

The `processAPI()` function in restAPI.ino grew into a ~287-line monolithic if-else chain dispatching 10+ API resources. Each branch mixed routing logic with business logic, making it difficult to:

1. Add new API endpoints without touching the central function
2. Verify that all endpoints follow the same response patterns (CORS, Content-Type, error handling)
3. Understand the full API surface at a glance
4. Maintain consistent HTTP method handling across resources

The pattern also violated ADR-035's uniformity goal — some resources used `httpServer.send()`, others used streaming, and error responses were inconsistent.

## Decision

**Replace the monolithic if-else chain with a function-pointer dispatch table.**

Each API resource is handled by a standalone function with a uniform signature:

```cpp
typedef void (*ApiResourceHandler)(
  const char words[][API_WORD_LEN],  // parsed URI segments
  uint8_t wordCount,                  // number of segments
  HTTPMethod method,                  // GET, POST, PUT, DELETE, OPTIONS
  const char* originalURI             // raw request URI for logging
);
```

Routes are defined in a static table with PROGMEM segment names:

```cpp
struct ApiRoute {
  PGM_P segment;               // URI segment to match (e.g., "health")
  ApiResourceHandler handler;   // function pointer
};

static const ApiRoute kV2Routes[] = {
  { kRouteHealth,      handleHealth },
  { kRouteSettings,    handleSettings },
  { kRouteSensors,     handleSensors },
  { kRouteDevice,      handleDevice },
  { kRouteFlash,       handleFlash },
  { kRoutePic,         handlePic },
  { kRouteFirmware,    handleFirmware },
  { kRouteFilesystem,  handleFilesystem },
  { kRouteOtgw,        handleOtgw },
  { kRouteWebhook,     handleWebhook },
  { nullptr, nullptr }  // sentinel
};
```

Dispatch is a simple loop:

```cpp
for (const ApiRoute* r = kV2Routes; r->segment != nullptr; r++) {
  if (strcmp_P(words[3], r->segment) == 0) {
    r->handler(words, wc, method, originalURI);
    return;
  }
}
// 404 if no match
```

**Common patterns extracted as shared helpers:**
- `sendApiOptions()` — uniform CORS preflight response
- `handleCommandSubmit()` — POST command-to-queue pattern (used by /otgw and others)

## Alternatives Considered

### Alternative 1: Keep if-else chain, just refactor bodies
Extract handler bodies into functions but keep the if-else routing.

**Why not chosen:** Still couples routing to a central function. Adding a resource still means editing processAPI(). The dispatch table completely decouples registration from dispatch.

### Alternative 2: ESP8266WebServer route registration (httpServer.on())
Register each route directly with the web server using `httpServer.on("/api/v2/resource", handler)`.

**Why not chosen:** The API uses a parsed word-array pattern where the URI is split into segments for sub-resource routing. The built-in `httpServer.on()` doesn't support this parsing. Also, routes need to share the same word-parsing logic, which the dispatch table preserves.

### Alternative 3: std::map or HashMap for routing
Use an associative container keyed by route string.

**Why not chosen:** ESP8266 has limited STL support, `std::map` uses dynamic allocation (violates ADR-004), and the number of routes (~10) is small enough that linear scan is faster than hash lookup due to cache effects.

## Consequences

### Positive
- **Single point of truth:** All API routes visible in one table
- **Uniform handler signature:** Every resource follows the same contract
- **Easy extensibility:** Adding a new route = one function + one table entry
- **Smaller processAPI():** ~40 lines of dispatch logic vs ~287 lines
- **PROGMEM route names:** String literals in flash, not RAM

### Negative
- **Slight indirection:** Function pointers add one level of indirection (negligible on ESP8266)
- **Handler isolation:** Handlers can't easily share local variables (solved by shared helpers)

## Implementation

Refactored in P2 of the C++ refactoring plan (restAPI.ino):
- 10 handler functions extracted: handleHealth, handleSettings, handleSensors, handleDevice, handleFlash, handlePic, handleFirmware, handleFilesystem, handleOtgw, handleWebhook
- 2 shared helpers: sendApiOptions(), handleCommandSubmit()
- processAPI() reduced to dispatch loop + 404 fallback

## Related Decisions
- ADR-035: RESTful API Compliance Strategy (design guidelines)
- ADR-019: REST API Versioning Strategy (v1/v2 URI structure)
- ADR-004: Static Buffer Allocation (PROGMEM route names)
- ADR-009: PROGMEM String Literals (route constants in flash)
