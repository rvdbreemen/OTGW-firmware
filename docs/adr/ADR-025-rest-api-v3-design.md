# ADR-025: REST API v3 Design

## Status
Accepted

## Context

The OTGW-firmware currently has three REST API versions:
- **v0** (legacy): Basic endpoints with inconsistent patterns
- **v1** (standard): More structured but missing proper REST semantics
- **v2** (optimized): Only `/otgw/otmonitor` endpoint with improved JSON format

### Problems with Current API Versions

1. **Incomplete HTTP Method Support**
   - POST and PUT used interchangeably (should be differentiated)
   - No PATCH support for partial updates
   - No DELETE operations
   - OPTIONS not implemented (breaks CORS preflight)
   - HEAD not supported

2. **Mixed Architectural Patterns**
   - Combines resource-oriented and RPC-style endpoints
   - `/api/v1/otgw/command/{command}` is RPC-style
   - Inconsistent URI structure

3. **Inconsistent Error Handling**
   - Some endpoints return plain text errors
   - No structured error format
   - Missing error codes for programmatic handling
   - Incomplete HTTP status code usage

4. **Limited REST Maturity**
   - Currently at Richardson Maturity Model Level 1-2
   - No HATEOAS (hypermedia) support
   - Not self-documenting
   - No discoverability

5. **Missing Modern Features**
   - No ETag support for caching
   - No pagination for large collections
   - No query parameter filtering
   - No rate limiting

### Requirements

- **Backward Compatibility:** v0, v1, v2 must remain unchanged
- **RESTful Compliance:** Achieve Richardson Maturity Level 3
- **Memory Constraints:** ESP8266 has ~40KB usable RAM
- **HTTP-Only:** Per ADR-003, no HTTPS/TLS
- **Platform Optimization:** Maintain PROGMEM usage (ADR-004)

## Decision

We will implement a **new REST API version 3 (v3)** that achieves full RESTful compliance while maintaining 100% backward compatibility with existing versions.

### Key Design Principles

#### 1. Resource-Oriented Architecture

All endpoints represent resources (nouns), not actions (verbs):

```
✅ GOOD: POST /api/v3/otgw/commands
❌ BAD:  POST /api/v3/otgw/sendCommand
```

#### 2. Proper HTTP Method Semantics

- **GET:** Retrieve resource(s), idempotent, cacheable
- **POST:** Create new resource, not idempotent
- **PUT:** Replace entire resource, idempotent
- **PATCH:** Update part of resource, not necessarily idempotent
- **DELETE:** Remove resource, idempotent
- **OPTIONS:** Return allowed methods (CORS preflight)
- **HEAD:** Same as GET but no body (existence check)

#### 3. Resource Hierarchy

```
/api/v3/
├── system/              # Device system information
│   ├── info             # Hardware/software details
│   ├── health           # Health check endpoint
│   ├── time             # Current time
│   ├── network          # Network status
│   └── stats            # Runtime statistics
├── config/              # Configuration resources
│   ├── settings         # All settings
│   ├── mqtt             # MQTT configuration
│   └── ntp              # NTP configuration
├── otgw/                # OpenTherm Gateway
│   ├── status           # Current status
│   ├── messages/        # OpenTherm messages
│   │   └── {id}         # Individual message
│   ├── thermostat       # Thermostat data
│   ├── boiler           # Boiler data
│   └── commands         # Command queue
├── pic/                 # PIC firmware
│   ├── firmware         # Firmware info
│   └── flash/status     # Flash status
├── sensors/             # Sensors
│   ├── dallas/          # Dallas sensors
│   │   └── {address}    # Individual sensor
│   └── s0               # S0 counter
└── export/              # Export formats
    ├── telegraf         # Telegraf format
    ├── otmonitor        # OTmonitor format
    └── prometheus       # Prometheus (future)
```

#### 4. Structured Error Responses

All errors return JSON with consistent structure:

```json
{
  "error": {
    "code": "INVALID_MESSAGE_ID",
    "message": "Message ID must be between 0 and 127",
    "details": {
      "provided": "256",
      "min": 0,
      "max": 127
    },
    "timestamp": 1736899200,
    "path": "/api/v3/otgw/messages/256"
  }
}
```

Error code categories:
- `SYSTEM_*` - System errors (heap, filesystem)
- `INVALID_*` - Validation errors
- `RESOURCE_*` - Resource errors (not found, unavailable)
- `RATE_LIMIT_EXCEEDED` - Rate limiting

#### 5. HATEOAS Support

All responses include `_links` for navigation:

```json
{
  "system": {
    "status": "online",
    "uptime": 86400,
    "_links": {
      "self": {"href": "/api/v3/system"},
      "info": {"href": "/api/v3/system/info"},
      "health": {"href": "/api/v3/system/health"}
    }
  }
}
```

#### 6. Standard HTTP Headers

```
Content-Type: application/json; charset=utf-8
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD
Access-Control-Allow-Headers: Content-Type, If-None-Match, If-Match
X-OTGW-Version: 1.0.0-rc6
X-OTGW-Heap-Free: 12345
ETag: "abc123" (for cacheable resources)
Cache-Control: no-cache | max-age=3600
```

#### 7. Proper Status Codes

```
2xx Success:
  200 OK               - Successful GET, PATCH, PUT, DELETE
  201 Created          - Successful POST creating resource
  202 Accepted         - Async operation accepted
  204 No Content       - Successful with no body

3xx Redirection:
  304 Not Modified     - Resource unchanged (ETag)

4xx Client Errors:
  400 Bad Request      - Malformed request
  404 Not Found        - Resource doesn't exist
  405 Method Not Allowed - Wrong HTTP method
  409 Conflict         - State conflict
  413 Payload Too Large
  414 URI Too Long
  422 Unprocessable Entity - Semantic errors
  429 Too Many Requests

5xx Server Errors:
  500 Internal Server Error
  503 Service Unavailable - Low heap
```

#### 8. Pagination Support

For large collections:

```
GET /api/v3/otgw/messages?page=1&per_page=20

Response:
{
  "messages": [...],
  "pagination": {
    "page": 1,
    "per_page": 20,
    "total": 128,
    "total_pages": 7
  },
  "_links": {
    "first": {"href": "...?page=1&per_page=20"},
    "next": {"href": "...?page=2&per_page=20"},
    "last": {"href": "...?page=7&per_page=20"}
  }
}
```

#### 9. Query Parameter Filtering

```
GET /api/v3/otgw/messages?label=roomtemperature
GET /api/v3/otgw/messages?category=temperature
GET /api/v3/sensors/dallas?connected=true
```

#### 10. ETag Caching

Supports conditional requests:

```
GET /api/v3/system/info
ETag: "abc123"

GET /api/v3/system/info
If-None-Match: "abc123"
→ 304 Not Modified (no body)
```

### Implementation Strategy

1. **New Module:** Create `restAPI_v3.ino` for v3 endpoints
2. **Router Integration:** Add v3 routing to existing `processAPI()`
3. **Helper Functions:** Extend `jsonStuff.ino` with v3 helpers
4. **Memory Efficiency:** 
   - All strings use PROGMEM (F() or PSTR())
   - Static buffer allocation per ADR-004
   - Response streaming for large payloads
5. **Backward Compatibility:**
   - No changes to existing v0/v1/v2 code
   - All versions coexist
   - v3 is additive only

### Example: PATCH vs PUT

```cpp
// PATCH - Update only specified fields
PATCH /api/v3/config/mqtt
{
  "broker": "192.168.1.100"
}
→ Updates only broker, other fields unchanged

// PUT - Replace entire resource
PUT /api/v3/config/mqtt
{
  "broker": "192.168.1.100",
  "port": 1883,
  "user": "otgw",
  ... all fields required
}
→ Replaces entire MQTT configuration
```

### Example: Command Resource (not RPC)

```cpp
// OLD (v1): RPC-style
POST /api/v1/otgw/command/TT=20.5

// NEW (v3): Resource-oriented
POST /api/v3/otgw/commands
{
  "command": "TT=20.5",
  "description": "Set temperature override"
}

Response: 201 Created
{
  "command": "TT=20.5",
  "status": "queued",
  "queue_position": 2,
  "_links": {
    "self": {"href": "/api/v3/otgw/commands/12345"},
    "queue": {"href": "/api/v3/otgw/commands"}
  }
}
```

## Consequences

### Benefits

✅ **Full REST Compliance**
- Achieves Richardson Maturity Level 3
- Industry-standard API design
- Better than 90% of IoT device APIs

✅ **Improved Developer Experience**
- Self-documenting with HATEOAS
- Consistent error handling
- Predictable patterns
- Clear documentation

✅ **Better Performance**
- ETag reduces redundant transfers
- Conditional requests save bandwidth
- Caching improves responsiveness

✅ **Future-Proof**
- Extensible design
- No breaking changes needed
- Clear deprecation path for v0

✅ **Backward Compatible**
- All existing clients continue working
- Gradual migration possible
- No forced upgrades

✅ **Platform-Appropriate**
- Respects ESP8266 memory limits
- PROGMEM usage maintained
- Static buffer allocation
- HTTP-only per ADR-003

### Trade-offs

⚠️ **Increased Flash Usage**
- Estimated +15-20KB flash for v3 code
- Acceptable: ESP8266 has 4MB flash

⚠️ **Slightly Higher RAM Usage**
- Estimated +2-3KB RAM for routing tables
- Mitigated: Static allocation, PROGMEM strings
- Still within safe limits (heap check: 4096 bytes minimum)

⚠️ **Development Effort**
- 60 working days estimated
- Mitigated: Incremental approach, can stop at any phase

⚠️ **More Complex Routing**
- More endpoints to route
- Mitigated: Modular design, clear separation

### Risks

1. **Memory Constraints**
   - Risk: v3 code exceeds RAM limits
   - Mitigation: Continuous profiling, static buffers, PROGMEM

2. **Backward Compatibility**
   - Risk: Accidental breakage of v0/v1/v2
   - Mitigation: Separate module, comprehensive regression tests

3. **Scope Creep**
   - Risk: Feature additions beyond plan
   - Mitigation: Phased approach, optional features marked

### Deprecation Strategy

- **v3 (1.1.0+):** New standard, recommended for all new development
- **v2:** Maintained indefinitely (limited to otmonitor endpoint)
- **v1:** Maintained for 2+ years, eventual deprecation warnings
- **v0:** Deprecation notice in v1.1.0, removal in v2.0.0

### Not Included (Out of Scope)

❌ **HTTPS/TLS** - Per ADR-003 (HTTP-only architecture)
❌ **Authentication** - Local network deployment model
❌ **GraphQL** - Future consideration, separate ADR
❌ **WebSockets** - Already handled separately (ADR-005)
❌ **Bulk Operations** - Future enhancement
❌ **Field Projection** - Future enhancement

## Related

- **ADR-003:** HTTP-Only No HTTPS - Establishes HTTP-only constraint
- **ADR-004:** Static Buffer Allocation - Memory management patterns
- **ADR-005:** WebSocket Real-Time Streaming - Separate real-time channel
- **ADR-018:** ArduinoJson Data Interchange - JSON library choice
- **ADR-019:** REST API Versioning Strategy - Versioning approach
- **Implementation Plan:** `docs/planning/REST_API_V3_MODERNIZATION_PLAN.md`
- **Resource Model:** `docs/api/v3/RESOURCE_MODEL.md`
- **Error Responses:** `docs/api/v3/ERROR_RESPONSES.md`
- **HTTP Status Codes:** `docs/api/v3/HTTP_STATUS_CODES.md`
- **Response Headers:** `docs/api/v3/RESPONSE_HEADERS.md`

## References

- [RFC 7231](https://tools.ietf.org/html/rfc7231) - HTTP/1.1 Semantics and Content
- [RFC 5789](https://tools.ietf.org/html/rfc5789) - PATCH Method for HTTP
- [RFC 7232](https://tools.ietf.org/html/rfc7232) - HTTP/1.1 Conditional Requests
- [Richardson Maturity Model](https://martinfowler.com/articles/richardsonMaturityModel.html)
- REST API Design Rulebook (O'Reilly)
