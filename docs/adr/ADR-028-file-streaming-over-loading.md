# ADR-028: File Streaming Over Loading for Memory Safety

**Status:** Accepted  
**Date:** 2026-02-01  
**Updated:** 2026-02-01 (Initial version)  
**Related to:** ADR-004 (Static Buffer Allocation), ADR-009 (PROGMEM String Literals)

## Context

The ESP8266 has severely limited RAM (~40KB available after core libraries). Loading large files entirely into memory using `File.readString()` or similar methods can cause:

1. **Memory Exhaustion**: Files >5KB can consume >50% of available RAM
2. **Heap Fragmentation**: String class allocations/reallocations fragment heap
3. **Crashes**: Multiple concurrent requests can exhaust memory causing watchdog resets
4. **Service Degradation**: Memory pressure affects all services (HTTP, MQTT, WebSocket)

### The Bug That Led to This ADR

**Commit 2e935543 (2026-02-01)** fixed a critical bug where `index.html` (~11KB) was loaded entirely into RAM:

```cpp
// PROBLEMATIC CODE (before fix)
String html = f.readString();  // Loads 11KB into heap
html.replace("old", "new");    // Allocation 2: 11KB + growth
html.replace("foo", "bar");    // Allocation 3: 11KB + growth
httpServer.send(200, type, html);  // Peak usage: >22KB (>50% of RAM)
```

**Impact:**
- 3 duplicate route handlers (/, /index, /index.html)
- 22KB+ peak memory per request
- Crashes with version mismatches or concurrent requests
- Code duplication (3x maintenance burden)

**Fix Applied:**
- Streaming with chunked transfer encoding (<1KB memory per request)
- Lambda deduplication (single implementation)
- Static caching for expensive file I/O operations
- 95% memory reduction

**Quality Assessment:** ⭐⭐⭐⭐⭐ (5/5) - Exemplary bug fix

### Codebase Analysis

Audit of all `readString()`/`readStringUntil()` usage (2026-02-01):

| File | Line | Content | Size | Status |
|------|------|---------|------|--------|
| helperStuff.ino | 192 | /reboot_count.txt | ~10 bytes | ✅ Safe |
| helperStuff.ino | 360 | /reboot_log.txt | ~2.8 KB | ✅ Safe (bounded) |
| helperStuff.ino | 475, 506 | /version.hash | ~40 bytes | ✅ Safe (cached) |
| FSexplorer.ino | 105 | /index.html (streaming) | ~11 KB | ✅ Safe (streamed) |
| FSexplorer.ino | 280 | .ver files | ~32 bytes | ✅ Safe |
| OTGW-Core.ino | 258 | Serial stream | Unbounded | ⚠️ **NEEDS FIX** |

## Decision

**MANDATORY: Never load files >2KB entirely into RAM. Always use streaming for large files.**

### File Size Thresholds

| Size | Strategy | Risk Level |
|------|----------|------------|
| **<1KB** | Can use `readString()` if necessary | ⚠️ Low |
| **1-5KB** | Prefer streaming; `readString()` only if no alternative | ⚠️ Medium |
| **>5KB** | MUST use streaming, NEVER load into memory | 🔴 High |
| **>10KB** | CRITICAL - Always stream, can cause crash if loaded | 🔴 Critical |

### Implementation Patterns

#### Pattern 1: Direct Streaming (Unmodified Files)

**Use when:** File doesn't need modification

```cpp
File f = LittleFS.open("/file.html", "r");
if (!f) {
  httpServer.send(404, F("text/plain"), F("File not found"));
  return;
}
httpServer.streamFile(f, F("text/html; charset=UTF-8"));
f.close();
```

**Memory impact:** Minimal (~few hundred bytes)

#### Pattern 2: Chunked Transfer Encoding (Modified Files)

**Use when:** File needs content modification

```cpp
File f = LittleFS.open("/file.html", "r");
if (!f) {
  httpServer.send(404, F("text/plain"), F("File not found"));
  return;
}

httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
httpServer.send(200, F("text/html; charset=UTF-8"), F(""));

while (f.available()) {
  String line = f.readStringUntil('\n');
  
  // Modify line if needed (use indexOf() before replace() for efficiency)
  if (line.indexOf(F("src=\"./index.js\"")) >= 0) {
    line.replace(F("src=\"./index.js\""), "src=\"./index.js?v=" + version);
  }
  
  httpServer.sendContent(line);
  if (f.available() || line.length() > 0) {
    httpServer.sendContent(F("\n"));
  }
}
httpServer.sendContent(F("")); // End chunked stream
f.close();
```

**Memory impact:** ~100-500 bytes per line (95% reduction vs full file)

#### Pattern 3: Lambda Deduplication

**Use when:** Multiple routes serve the same content

```cpp
// GOOD - Single handler for multiple routes
auto sendIndex = []() {
  // Implementation here
};

httpServer.on("/", sendIndex);
httpServer.on("/index", sendIndex);
httpServer.on("/index.html", sendIndex);

// BAD - Duplicate code for each route (3x bugs, 3x maintenance)
httpServer.on("/", []() { /* duplicate code */ });
httpServer.on("/index", []() { /* duplicate code */ });
httpServer.on("/index.html", []() { /* duplicate code */ });
```

**Benefits:** Single point of maintenance, reduced binary size, fewer bugs

#### Pattern 4: Static Caching

**Use when:** Expensive file I/O operations read static data

```cpp
String getFilesystemHash() {
  static String _githash = ""; // Cache persists across function calls
  
  if (_githash.length() > 0) return _githash; // Return cached value
  
  // Read from file only on first call
  File f = LittleFS.open("/version.hash", "r");
  if (f && f.available()) {
    _githash = f.readStringUntil('\n');
    _githash.trim();
    f.close();
  }
  return _githash;
}

// BAD - Reading file on every call
String getFilesystemHash() {
  File f = LittleFS.open("/version.hash", "r");
  String hash = f.readStringUntil('\n');
  f.close();
  return hash;
}
```

**Benefits:** Eliminates repeated file I/O, reduces LittleFS overhead

#### Pattern 5: Bounded Serial/Stream Reading

**Use when:** Reading from Serial, network streams, or unbounded sources

```cpp
// GOOD - Size limit with validation
String line = OTGWSerial.readStringUntil('\n');
if (line.length() > MAX_RESPONSE_SIZE) {  // e.g., 512 bytes
  OTGWDebugTln(F("ERROR: Response too long, truncating"));
  line = line.substring(0, MAX_RESPONSE_SIZE);
}
line.trim();

// BAD - No size limit, can exhaust memory
String line = OTGWSerial.readStringUntil('\n');
```

**Why:** Serial devices can send malformed data without newlines, consuming all heap

### Performance Optimizations

1. **Use `indexOf()` before `replace()`** - Avoid unnecessary allocations:
   ```cpp
   if (line.indexOf(F("search")) >= 0) {  // Check first
     line.replace(F("old"), "new");       // Only modify if needed
   }
   ```

2. **Limit String scope** - Let variables go out of scope quickly:
   ```cpp
   while (f.available()) {
     String line = f.readStringUntil('\n');  // Scoped to loop iteration
     // Process line
   } // line destroyed here
   ```

3. **Prefer char buffers** for fixed-size strings:
   ```cpp
   char buffer[64];
   f.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
   buffer[sizeof(buffer) - 1] = '\0';
   ```

## Alternatives Considered

### Alternative 1: Load Entire File with String Class

**Pros:**
- Simple code: `String html = f.readString();`
- Easy content modification
- Familiar pattern

**Cons:**
- Consumes entire file size in RAM
- String operations cause heap fragmentation
- Multiple concurrent requests crash device
- Not scalable as features added

**Why not chosen:** Memory exhaustion causes crashes. This bug (commit 2e93554) proved the pattern is unsafe.

### Alternative 2: Pre-process Files at Build Time

**Pros:**
- No runtime modification needed
- Can serve static files directly
- Faster response times

**Cons:**
- Can't inject runtime values (version hash, settings)
- Requires build-time templating system
- Less flexible for dynamic content
- Doesn't solve general file serving problem

**Why not chosen:** Runtime flexibility needed for version mismatches, cache-busting, dynamic configuration.

### Alternative 3: Increase Buffer Size, Use DynamicJsonDocument

**Pros:**
- Could handle larger files
- More headroom for operations

**Cons:**
- ESP8266 RAM is fixed at ~40KB
- Doesn't solve root cause
- Just delays the problem
- Makes concurrent requests worse

**Why not chosen:** Can't add RAM to ESP8266. Must work within constraints.

## Consequences

### Benefits

1. **Stability:** No more memory exhaustion crashes
2. **Scalability:** Can handle concurrent requests
3. **Performance:** Reduced heap fragmentation
4. **Maintainability:** Lambda deduplication reduces code
5. **Predictability:** Bounded memory usage

### Trade-offs

1. **Complexity:** Streaming code is more verbose than `readString()`
2. **Debugging:** Harder to inspect streamed content
3. **Learning curve:** Developers must understand patterns

### Migration Strategy

**Existing code:**
1. Audit all `readString()` usage (completed 2026-02-01)
2. Fix OTGW-Core.ino serial reading (bounded size)
3. Document safe vs unsafe patterns

**New code:**
1. Copilot instructions enforce patterns
2. Code review checklist includes file size checks
3. Evaluation framework flags violations

### Risks and Mitigation

**Risk 1:** Developers unfamiliar with patterns make mistakes

**Mitigation:**
- Comprehensive Copilot instructions with examples
- Quick reference guide for common patterns
- Code review enforcement
- Evaluation framework (`evaluate.py`) checks

**Risk 2:** Edge cases not covered by patterns

**Mitigation:**
- Document exceptions and justifications
- Case-by-case analysis in code review
- Update ADR as new patterns emerge

## Implementation Evidence

### Fixed Bug (Commit 2e93554)

**Files modified:**
- `FSexplorer.ino`: Implemented streaming with lambda deduplication
- `helperStuff.ino`: Added static caching for `getFilesystemHash()`

**Results:**
- Memory: 22KB+ → <1KB per request (95% reduction)
- Code: -15 lines net (eliminated duplication)
- Performance: Faster response, better concurrency

### Remaining Work (Commit TBD)

**OTGW-Core.ino line 258:** Add size limit to serial reading
```cpp
String line = OTGWSerial.readStringUntil('\n');
if (line.length() > 512) {  // Bounded response size
  OTGWDebugTln(F("ERROR: Response too long"));
  line = "";
}
```

## Related Documentation

- **Bug Fix Assessment:** `docs/reviews/2026-02-01_memory-management-bug-fix/BUG_FIX_ASSESSMENT.md`
- **Quick Reference:** `docs/reviews/2026-02-01_memory-management-bug-fix/QUICK_REFERENCE.md`
- **Executive Summary:** `docs/reviews/2026-02-01_memory-management-bug-fix/EXECUTIVE_SUMMARY.md`
- **Copilot Instructions:** `.github/copilot-instructions.md` (lines 193-320)

## Related ADRs

- **ADR-004:** Static Buffer Allocation Strategy
- **ADR-009:** PROGMEM Usage for String Literals
- **ADR-023:** Filesystem Explorer HTTP API
- **ADR-026:** Conditional JavaScript Cache Busting

## References

- **Bug Fix Commit:** 2e935543b9381566d77545559bffdde98475a3e7
- **ESP8266 Arduino Core:** https://arduino-esp8266.readthedocs.io/
- **ESP8266WebServer Documentation:** https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer

---

**This ADR formalizes the file streaming pattern as the standard approach for ESP8266 file serving, based on real-world production bug experience.**
