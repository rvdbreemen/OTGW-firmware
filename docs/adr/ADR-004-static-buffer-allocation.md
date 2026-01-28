# ADR-004: Static Buffer Allocation Strategy

**Status:** Accepted  
**Date:** 2020-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)  
**Enhanced:** 2025-12-15 (v1.0.0 heap protection system)

## Context

The ESP8266 has extremely limited RAM (~40KB usable after Arduino core). Dynamic memory allocation patterns led to heap fragmentation and crashes, especially under load with multiple concurrent services (HTTP, MQTT, WebSocket).

**Problem symptoms:**
- Random crashes after hours/days of operation
- Out-of-memory errors during MQTT publish
- WebSocket disconnections under load
- System instability with multiple simultaneous clients

**Root causes identified:**
1. `String` class uses dynamic allocation (resize on append)
2. `DynamicJsonDocument` allocates/deallocates variable-sized buffers
3. HTTP response buffers resize based on content
4. MQTT message buffers grow with message size
5. WebSocket per-client buffers allocated dynamically

## Decision

**Adopt static buffer allocation with bounded sizes throughout the codebase.**

**Core principles:**
1. **Use `char[]` arrays instead of `String` class** in performance-critical code
2. **Fixed-size buffers** with explicit bounds checking
3. **PROGMEM for all string literals** (F() macro, PSTR() macro)
4. **Compile-time buffer sizing** based on maximum expected values
5. **Heap monitoring** with 4-level alerting system (v1.0.0+)
6. **Adaptive throttling** to prevent heap exhaustion

**Key buffer sizes (v1.0.0):**
```cpp
// Fixed buffer allocations
#define CSTR_SIZE 256              // General string buffer
#define JSON_BUFFER_SIZE 1536      // ArduinoJson document
#define MQTT_BUFFER_SIZE 1200      // Maximum MQTT message
#define WEBSOCKET_BUFFER_SIZE 256  // Per-client WebSocket buffer (reduced from 512)
#define HTTP_API_BUFFER_SIZE 256   // HTTP response streaming (reduced from 1024)
#define CMSG_SIZE 512              // OpenTherm command messages
```

**Heap monitoring system:**
```cpp
// Heap health levels (v1.0.0)
#define HEAP_CRITICAL 3000   // <3KB: Emergency mode
#define HEAP_WARNING 5000    // 3-5KB: Throttle aggressively  
#define HEAP_LOW 8000        // 5-8KB: Reduce message rates
// >8KB: Normal operation
```

## Alternatives Considered

### Alternative 1: Continue Using String Class
**Pros:**
- Convenient API (append, substring, etc.)
- No manual memory management
- Familiar to Arduino developers

**Cons:**
- Dynamic allocation causes heap fragmentation
- Hidden allocations hard to track
- Memory leaks if not careful
- Performance overhead
- Crashes under load

**Why not chosen:** Heap fragmentation led to crashes in production. The convenience is not worth the instability.

### Alternative 2: Smart Pointers (std::unique_ptr, std::shared_ptr)
**Pros:**
- Automatic memory management
- Prevents leaks
- Modern C++ pattern

**Cons:**
- Still uses dynamic allocation
- Overhead for reference counting (shared_ptr)
- Limited ESP8266 Arduino STL support
- Doesn't solve fragmentation
- Memory overhead for control blocks

**Why not chosen:** Doesn't address the root cause (fragmentation). Adds complexity without solving the problem.

### Alternative 3: Memory Pool Allocator
**Pros:**
- Reduces fragmentation
- Predictable allocation patterns
- Can tune pool sizes

**Cons:**
- Complex to implement correctly
- Fixed pool sizes may waste memory
- Requires significant refactoring
- Debugging becomes harder
- Not standard in Arduino ecosystem

**Why not chosen:** Too complex for the benefits. Static allocation is simpler and more predictable.

### Alternative 4: External PSRAM/SPI RAM
**Pros:**
- More memory available
- Reduces pressure on internal RAM

**Cons:**
- Requires hardware modification
- Not available on NodeMCU/Wemos D1 mini
- Slower access than internal RAM
- Breaks compatibility with existing hardware
- Additional cost

**Why not chosen:** Requires incompatible hardware changes. Not feasible for existing deployed devices.

## Consequences

### Positive
- **Stability:** Days → Weeks/Months of continuous operation (v1.0.0)
- **Predictable memory usage:** No unexpected allocations
- **No heap fragmentation:** Static buffers never resize
- **Performance:** No allocation overhead in hot paths
- **Debuggability:** Memory usage visible at compile time
- **Heap monitoring:** Real-time visibility into memory health (v1.0.0)
- **Adaptive throttling:** System self-protects under memory pressure (v1.0.0)

**Memory savings (v1.0.0):**
- WebSocket buffers: 768 bytes saved (512→256 per client × 3 clients)
- HTTP API streaming: 768 bytes saved (1024→256)
- MQTT optimizations: 200-400 bytes saved
- PROGMEM strings: ~2,000 bytes saved
- **Total:** 3,130-3,730 bytes (7.8-9.3% of available RAM)
- **With optional features:** Up to 5,234 bytes (13.1%)

### Negative
- **Code verbosity:** Manual buffer management is more verbose than String class
  - Mitigation: Helper macros (CSTR macro for null safety)
- **Buffer overflow risk:** Must manually check bounds
  - Mitigation: Use `snprintf_P()`, `strlcpy()` with size limits
- **Fixed limits:** Maximum message sizes are hard-coded
  - Accepted: Trade-off for stability. Limits documented and validated.
- **Developer discipline required:** Easy to make mistakes
  - Mitigation: Code reviews, evaluation framework (`evaluate.py`), copilot instructions

### Risks & Mitigation
- **Buffer overflows:** Writing past buffer end corrupts memory
  - **Mitigation:** Always use bounded functions (`snprintf`, `strlcpy`, never `strcpy`)
  - **Mitigation:** Evaluation framework checks for unsafe patterns
- **Truncation:** Data may be cut off if buffer too small
  - **Mitigation:** Buffer sizes chosen based on maximum expected values
  - **Mitigation:** Log warnings when truncation occurs
- **PROGMEM errors:** Reading from PROGMEM requires special functions
  - **Mitigation:** Always use `_P` variants (`strcmp_P`, `snprintf_P`)
  - **Mitigation:** Code review checklist enforces PROGMEM usage

## Implementation Patterns

**String handling:**
```cpp
// BAD - Dynamic allocation
String message = "Hello ";
message += variable;
message += " World";

// GOOD - Static buffer
char message[CSTR_SIZE];
snprintf_P(message, sizeof(message), PSTR("Hello %s World"), variable);
```

**PROGMEM strings:**
```cpp
// BAD - Wastes RAM
DebugTln("Starting WiFi");

// GOOD - Keeps string in flash
DebugTln(F("Starting WiFi"));
DebugTf(PSTR("Value: %d\r\n"), value);
```

**Null safety macro (v1.0.0):**
```cpp
// Prevents crashes from empty String objects
#define CSTR(x) ((x).c_str() && (x).c_str()[0] != '\0' ? (x).c_str() : "")

// Usage
httpServer.send(200, F("text/html"), CSTR(htmlContent));
```

**Heap monitoring (v1.0.0):**
```cpp
uint32_t freeHeap = ESP.getFreeHeap();
if (freeHeap < HEAP_CRITICAL) {
  // Emergency mode: Block new connections, cleanup
} else if (freeHeap < HEAP_WARNING) {
  // Throttle: 5 msg/s → 2 msg/s
} else if (freeHeap < HEAP_LOW) {
  // Reduce: 20 msg/s → 5 msg/s
}
// Else: Normal operation
```

## Related Decisions
- ADR-001: ESP8266 Platform Selection (memory constraints)
- ADR-009: PROGMEM Usage for String Literals (RAM savings)
- ADR-003: HTTP-Only Network Architecture (memory for TLS not available)
- ADR-006: MQTT Integration Pattern (uses static buffers and chunked streaming)
- ADR-012: PIC Firmware Upgrade via Web UI (binary data parsing with bounded buffers)

## References
- Heap protection implementation: `OTGW-firmware.h` (CSTR macro, heap levels)
- Evaluation framework: `evaluate.py` (checks for String class overuse)
- Developer guidelines: `.github/copilot-instructions.md` (Memory Management section)
- Memory optimizations documentation: README.md (v1.0.0 features)
- Buffer sizes: `OTGW-firmware.h`, `webSocketStuff.ino`, `MQTTstuff.ino`
