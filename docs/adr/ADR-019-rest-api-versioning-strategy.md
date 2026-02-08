# ADR-019: REST API Versioning Strategy

**Status:** Accepted  
**Date:** 2020-06-01 (v1 introduced), 2024-01-01 (v2 introduced)  
**Updated:** 2026-01-28 (Documentation)

## Context

As the OTGW-firmware evolved, the REST API needed to provide new functionality and improve data formats. However, existing integrations (Home Assistant, scripts, mobile apps) depended on the original API structure.

**Challenges:**
- Breaking changes would break existing integrations
- New features required different data structures
- Performance improvements needed different response formats
- Users couldn't update all clients simultaneously

**Evolution timeline:**
- **v0 (original):** Legacy endpoints, basic JSON responses
- **v1 (2020):** Standardized structure, array-based responses `[{name, value}]`
- **v2 (2024):** Optimized format with units, map-based responses `{key: {value, unit}}`

**Example evolution:**
```
v0: /api/otmonitor (mixed format)
v1: /api/v1/otgw/data (array format)
v2: /api/v2/otgw/data (map format with metadata)
```

## Decision

**Adopt URL path-based API versioning with indefinite version support (no deprecation).**

**Implementation:**
- **Version prefix:** `/api/v{N}/` in URL path
- **Version coexistence:** All versions remain active indefinitely
- **No forced migration:** Old clients continue working
- **Version detection:** Parse version from URI word array
- **Shared backend:** Same data processing, different serialization per version

**Version differences:**
```cpp
// v1 response format
[
  {"name": "boiler_temp", "value": 65.2},
  {"name": "return_temp", "value": 55.1}
]

// v2 response format
{
  "boiler_temp": {"value": 65.2, "unit": "°C"},
  "return_temp": {"value": 55.1, "unit": "°C"}
}
```

## Alternatives Considered

### Alternative 1: Query Parameter Versioning
**Pros:**
- Single endpoint URL
- Familiar pattern (e.g., `?version=2`)
- Can set default version

**Cons:**
- Easy to omit parameter (defaults can break)
- Cache issues (same URL, different responses)
- Less discoverable
- No standard parameter name

**Why not chosen:** URL path versioning is more explicit and prevents accidental version mismatch.

### Alternative 2: Header-Based Versioning
**Pros:**
- Clean URLs
- RESTful purist approach
- Content negotiation standard

**Cons:**
- Not browser-friendly (can't test with URL)
- Requires custom headers
- Harder to debug
- Not discoverable via URL exploration

**Why not chosen:** Browser testing and URL discoverability are important for development and debugging.

### Alternative 3: Subdomain Versioning
**Pros:**
- Complete isolation
- Can run different code versions
- CDN-friendly

**Cons:**
- Requires DNS configuration
- Certificate complexity (HTTPS)
- Not feasible on embedded device
- Overkill for simple API

**Why not chosen:** Too complex for embedded device. Single IP address, no DNS control.

### Alternative 4: Breaking Changes with Migration Period
**Pros:**
- Cleaner codebase (one version)
- Forces updates
- Simpler maintenance

**Cons:**
- Breaks existing integrations
- Forces users to update all clients
- May cause downtime
- Loss of trust

**Why not chosen:** Stability is critical for home heating control. Breaking changes unacceptable.

### Alternative 5: Semantic Versioning with Deprecation
**Pros:**
- Clear communication of changes
- Planned obsolescence
- Industry standard

**Cons:**
- Forces eventual migration
- Requires deprecation timeline enforcement
- Support burden during transition
- May still break integrations

**Why not chosen:** No compelling reason to remove old versions on embedded device with ample flash space.

## Consequences

### Positive
- **Zero-downtime updates:** Users upgrade firmware without breaking integrations
- **Backward compatibility:** Old scripts/apps continue working indefinitely
- **Gradual migration:** Users can update clients at their own pace
- **API experimentation:** New features can be tested in new versions
- **Clear versioning:** Version explicit in URL, no ambiguity
- **Browser testable:** Can test all versions in browser address bar

### Negative
- **Code duplication:** Multiple response formatters for same data
  - Mitigation: Share data fetching, only serialize differently
- **Testing burden:** Must test all API versions
  - Mitigation: Automated tests cover all versions
- **Documentation overhead:** Must document all active versions
  - Mitigation: Clear migration guides between versions
- **No sunset policy:** Old versions live forever
  - Accepted: Flash space is not constrained (4MB available)

### Risks & Mitigation
- **Version proliferation:** Too many versions to maintain
  - **Mitigation:** Only create new version for breaking changes
  - **Current state:** 3 versions over 6 years is manageable
- **Inconsistent behavior:** Versions diverge in functionality
  - **Mitigation:** All versions access same underlying data
  - **Mitigation:** Only response format differs, not capabilities
- **Security patches:** Must patch all versions
  - **Mitigation:** Shared backend means one fix applies to all
- **Feature disparity:** New features only in latest version
  - **Accepted:** Encourages migration while maintaining compatibility

## Implementation Details

**Version detection:**
```cpp
void handleAPI() {
  // Parse URI into words
  // Example: /api/v1/otgw/data → ["api", "v1", "otgw", "data"]
  String uri[10];
  int uriCount = splitUri(httpServer.uri(), uri, 10);
  
  // Extract version (word[1])
  String version = uri[1];  // "v1", "v2", or empty for v0
  
  if (version == "v1") {
    handleV1Request(uri, uriCount);
  } else if (version == "v2") {
    handleV2Request(uri, uriCount);
  } else {
    handleV0Request(uri, uriCount);  // Legacy
  }
}
```

**Shared data fetching:**
```cpp
void handleOTGWData() {
  // Fetch data (version-independent)
  float boilerTemp = OTdata.Tboiler;
  float returnTemp = OTdata.Tret;
  // ... more data
  
  // Determine version from URI
  String version = getAPIVersion();
  
  if (version == "v2") {
    // v2 format: map with units
    DynamicJsonDocument doc(1536);
    doc["boiler_temp"]["value"] = boilerTemp;
    doc["boiler_temp"]["unit"] = "°C";
    doc["return_temp"]["value"] = returnTemp;
    doc["return_temp"]["unit"] = "°C";
    
    serializeJson(doc, httpServer);
  } else if (version == "v1") {
    // v1 format: array
    DynamicJsonDocument doc(1536);
    JsonArray arr = doc.to<JsonArray>();
    
    JsonObject obj1 = arr.createNestedObject();
    obj1["name"] = "boiler_temp";
    obj1["value"] = boilerTemp;
    
    JsonObject obj2 = arr.createNestedObject();
    obj2["name"] = "return_temp";
    obj2["value"] = returnTemp;
    
    serializeJson(doc, httpServer);
  } else {
    // v0 format: legacy
    sendV0Response(boilerTemp, returnTemp);
  }
}
```

**Version-specific endpoints:**
```cpp
// v0 (legacy)
httpServer.on("/api/otmonitor", handleOTMonitor);
httpServer.on("/api/otmonitor/v2", handleOTMonitorV2);

// v1 (standard)
httpServer.on("/api/v1/otgw/data", handleV1Data);
httpServer.on("/api/v1/otgw/status", handleV1Status);
httpServer.on("/api/v1/health", handleV1Health);

// v2 (optimized)
httpServer.on("/api/v2/otgw/data", handleV2Data);
httpServer.on("/api/v2/otgw/status", handleV2Status);
```

**Message ID validation (version-specific):**
```cpp
bool validateMessageID(int msgid, String version) {
  if (version == "v1" || version == "v2") {
    // Strict validation for modern versions
    return (msgid >= 0 && msgid <= 255);
  } else {
    // Lenient for v0 (legacy clients may send invalid IDs)
    return true;  // Accept all, log warning
  }
}
```

## API Version Comparison

| Feature | v0 | v1 | v2 |
|---------|----|----|-----|
| **Format** | Mixed | Array of objects | Map with metadata |
| **Units** | No | No | Yes |
| **Validation** | Lenient | Standard | Strict |
| **Documentation** | Minimal | Complete | Complete |
| **Performance** | Slow | Medium | Fast |
| **Recommended** | No | Yes | Yes (new code) |

**Migration path:**
```
v0 → v1: Change URL, update parser to handle array
v1 → v2: Change URL, update parser to handle map
```

## Version-Specific Features

**v0 only:**
- `/api/otmonitor` - OTmonitor-compatible JSON
- Legacy field names
- No validation

**v1 additions:**
- `/api/v1/health` - System health check
- `/api/v1/time` - Device time/NTP status
- `/api/v1/settings` - Configuration management
- Standard error responses

**v2 additions:**
- Units in responses (`{"value": 20.5, "unit": "°C"}`)
- Improved response times (optimized serialization)
- Batch operations support
- Metadata in responses (timestamp, version)

## Documentation Strategy

**API documentation:**
- `example-api/` directory contains examples for all versions
- README.md documents migration between versions
- Changelog notes which version introduced features

**Deprecation policy (none):**
- No versions are deprecated
- All versions receive security updates
- New features may only appear in latest version
- Users encouraged to migrate but not required

## Related Decisions
- ADR-018: ArduinoJson for Data Interchange (JSON serialization)
- ADR-006: MQTT Integration Pattern (separate versioning strategy)

## References
- Implementation: `restAPI.ino` (version routing and handlers)
- API examples: `example-api/` directory
- Migration guide: `docs/API_CHANGES_v1.0.0.md`
- Endpoint list: `restAPI.ino` (httpServer.on() calls)
