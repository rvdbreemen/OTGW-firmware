# ADR-009: PROGMEM Usage for String Literals

**Status:** Accepted  
**Date:** 2018-06-01 (Initial adoption)  
**Updated:** 2026-01-28 (Documentation)  
**Enforced:** 2020-01-01 (Mandatory via evaluation framework)

## Context

The ESP8266 has severely limited RAM:
- **Total SRAM:** ~80KB
- **After Arduino core:** ~40KB available
- **After WiFi stack:** ~20-25KB available for application

Every string literal declared normally consumes RAM:
```cpp
Serial.println("Starting WiFi");  // "Starting WiFi" stored in RAM (15 bytes)
```

With hundreds of debug messages, HTTP responses, and MQTT topics throughout the codebase, string literals were consuming **5-8KB of precious RAM** that was needed for runtime operations.

**Problem manifestation:**
- Heap exhaustion errors
- Crashes during MQTT publishing
- Out of memory during JSON serialization
- WebSocket disconnections
- System instability after hours of operation

## Decision

**MANDATORY: All string literals MUST use PROGMEM to store them in flash memory instead of RAM.**

**Implementation patterns:**
1. **Use `F()` macro** for functions that support it (String-compatible)
2. **Use `PSTR()` macro** for printf-style formatted strings
3. **Use `PROGMEM` keyword** for constant string arrays and tables
4. **Use `_P` variants** for string comparison functions

**Examples:**
```cpp
// Debug output
DebugTln(F("Starting WiFi"));                    // F() macro
DebugTf(PSTR("IP: %s\r\n"), ipAddress);         // PSTR() macro

// HTTP responses
httpServer.send(200, F("text/html"), content);   // F() macro

// String comparisons
if (strcmp_P(str, PSTR("value")) == 0) { }      // PSTR() + strcmp_P

// Constant arrays
const char header[] PROGMEM = "HTTP/1.1 200 OK\r\n";  // PROGMEM keyword
```

**Enforcement:**
- Evaluation framework (`evaluate.py`) scans for violations
- Code review checklist includes PROGMEM verification
- Copilot instructions mandate usage

## Alternatives Considered

### Alternative 1: Keep String Literals in RAM
**Pros:**
- Simpler code (no macros)
- Standard C/C++ patterns
- Faster string access (RAM vs flash)

**Cons:**
- Wastes 5-8KB of RAM
- Reduces available heap by 20-40%
- Causes crashes and instability
- Not sustainable as features added

**Why not chosen:** RAM is far too limited. This is non-negotiable for ESP8266 stability.

### Alternative 2: External SPI RAM
**Pros:**
- More RAM available
- Could keep strings in RAM

**Cons:**
- Requires hardware modification
- Not available on standard NodeMCU/Wemos
- Slower access than internal RAM
- Breaks compatibility
- Cost increase

**Why not chosen:** Incompatible with existing hardware. Not feasible.

### Alternative 3: Compressed Strings
**Pros:**
- Reduces storage size
- Could fit more in RAM

**Cons:**
- Decompression overhead (CPU and temporary RAM)
- Complexity
- Still consumes RAM for decompressed strings
- Doesn't solve the fundamental problem

**Why not chosen:** Adds complexity without solving RAM exhaustion.

### Alternative 4: String Table with Indices
**Pros:**
- Central string management
- Could optimize storage

**Cons:**
- Requires maintaining string table
- Index lookup overhead
- Error-prone (wrong index = wrong string)
- Doesn't reduce RAM usage
- Poor maintainability

**Why not chosen:** Doesn't address RAM consumption. Adds complexity.

## Consequences

### Positive
- **RAM savings:** ~5-8KB freed (20-40% of available heap)
- **Stability:** Eliminates most out-of-memory crashes
- **Scalability:** Can add features without exhausting RAM
- **Performance:** More heap available for runtime allocations
- **Predictable:** RAM usage is stable and measurable

**Measured impact (v1.0.0):**
- String literals: ~2,000 bytes moved to flash
- Combined with other optimizations: 3,130-3,730 bytes total savings
- Heap available increased from ~15KB to ~20KB typical

### Negative
- **Code verbosity:** Requires `F()` or `PSTR()` wrapper on every string
  - Accepted: Necessary trade-off for stability
- **Performance:** Flash access slower than RAM (~4x slower)
  - Accepted: String access is not performance-critical
  - Mitigation: Copy to RAM buffer if used repeatedly
- **Special functions:** Must use `_P` variants (strcmp_P, strcpy_P, etc.)
  - Mitigation: Copilot instructions document all patterns
- **Easy to forget:** Developers may forget to use macros
  - Mitigation: Evaluation framework catches violations
  - Mitigation: Code review checklist
- **Compile errors:** Wrong function variant causes compile failure
  - Accepted: Better compile error than runtime crash

### Risks & Mitigation
- **Missing _P function:** Not all string functions have _P variants
  - **Mitigation:** Create function overloads that accept PROGMEM types
  - **Example:** See `sendData()` overload pattern in copilot instructions
- **Binary data corruption:** Using string functions on binary data
  - **Mitigation:** Use `memcmp_P()` for binary, never `strncmp_P()` (see ADR-004)
- **Performance bottleneck:** Frequent flash access could slow system
  - **Accepted:** String access is infrequent; not a real issue
  - **Mitigation:** Cache in RAM if accessed in tight loop

## Implementation Patterns

**Debug output (most common):**
```cpp
// Simple message
DebugTln(F("WiFi connected"));

// Formatted output
DebugTf(PSTR("IP: %s, RSSI: %d\r\n"), ipAddress, rssi);

// Conditional debug
if (error) {
  DebugTf(PSTR("Error code: %d\r\n"), errorCode);
}
```

**String comparisons:**
```cpp
// Compare to PROGMEM literal
if (strcmp_P(value, PSTR("ON")) == 0) {
  // Match
}

// Case-insensitive compare
if (strcasecmp_P(field, PSTR("Hostname")) == 0) {
  // Field name match
}
```

**Constant string arrays:**
```cpp
// Array of strings
const char str1[] PROGMEM = "Option 1";
const char str2[] PROGMEM = "Option 2";
const char str3[] PROGMEM = "Option 3";

const char* const stringTable[] PROGMEM = {
  str1, str2, str3
};

// Read from table
char buffer[32];
strcpy_P(buffer, (char*)pgm_read_ptr(&stringTable[index]));
```

**Binary data (CRITICAL):**
```cpp
// CORRECT - Binary data comparison
const char banner[] PROGMEM = "\x1F\x8B\x08";  // gzip magic
if (memcmp_P(data, banner, sizeof(banner) - 1) == 0) {
  // Found gzip header
}

// WRONG - String functions on binary data cause crashes
if (strncmp_P(data, banner, sizeof(banner)) == 0) {  // DANGEROUS!
  // May crash (reads past buffer looking for null terminator)
}
```

**Function overloads:**
```cpp
// Original function (RAM strings)
void sendResponse(const char* message) {
  httpServer.send(200, "text/plain", message);
}

// Overload for PROGMEM strings
void sendResponse(const __FlashStringHelper* message) {
  // Read from PROGMEM and send
  char buffer[256];
  strncpy_P(buffer, (const char*)message, sizeof(buffer));
  httpServer.send(200, F("text/plain"), buffer);
}

// Usage
sendResponse(F("OK"));  // Uses PROGMEM overload
```

## Evaluation Framework Integration

**Checks performed by `evaluate.py`:**
1. Scans for bare string literals in function calls
2. Flags missing `F()` or `PSTR()` macros
3. Checks for `strcmp()` vs `strcmp_P()` usage
4. Validates `PROGMEM` keyword on const arrays
5. Identifies potential binary data bugs (strncmp_P on hex files)

**Example violations detected:**
```cpp
// FAIL: String literal without F()
DebugTln("Starting");

// PASS: Correct usage
DebugTln(F("Starting"));

// FAIL: strcmp on PROGMEM string
strcmp(value, "ON");

// PASS: Correct comparison
strcmp_P(value, PSTR("ON"));
```

## Related Decisions
- ADR-001: ESP8266 Platform Selection (RAM constraints)
- ADR-004: Static Buffer Allocation Strategy (overall memory management)

## References
- PROGMEM documentation: https://www.arduino.cc/reference/en/language/variables/utilities/progmem/
- ESP8266 memory layout: https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
- Implementation: All `.ino` files, `Debug.h`
- Evaluation framework: `evaluate.py` (PROGMEM checks)
- Copilot instructions: `.github/copilot-instructions.md` (PROGMEM section)
- Binary data safety: ADR-004, `versionStuff.ino`, `src/libraries/OTGWSerial/`
