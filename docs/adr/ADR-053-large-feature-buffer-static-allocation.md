# ADR-053: Large Feature Buffer Static Allocation

**Status:** Accepted  
**Date:** 2026-03-21  
**Supersedes:** ADR-004 (Static Buffer Allocation Strategy)

## Context

ADR-004 established that all buffers must be statically allocated to eliminate heap fragmentation on the ESP8266's ~40 KB RAM. However, the MQTT auto-discovery subsystem introduced a lazy-allocation pattern that violated ADR-004:

```cpp
// MQTTstuff.ino — introduced after ADR-004 was accepted
static MQTTAutoConfigBuffers* pMqttAutoConfigBuffers = nullptr;

static MQTTAutoConfigBuffers* getMqttAutoConfigBuffers() {
  if (!pMqttAutoConfigBuffers) {
    pMqttAutoConfigBuffers = new MQTTAutoConfigBuffers(); // ← heap allocation
  }
  return pMqttAutoConfigBuffers;
}
```

`MQTTAutoConfigBuffers` holds ~1400 bytes (`char line[1200]` + `char topic[200]`). The rationale was: if MQTT auto-discovery is never triggered, those 1400 bytes are never used. The code comment even noted "once allocated, kept permanently — acceptable for embedded."

This pattern is still a violation of ADR-004: the lazy allocation introduces a single `new` call that fragments the heap at an unpredictable moment (the first auto-discovery run, which can happen at any time during device operation), and adds nullable-pointer complexity for no practical benefit (auto-discovery runs on almost every device that has MQTT enabled).

## Decision

**All feature-specific working buffers must be declared as file-static globals, not heap-allocated — even when the feature is only used occasionally.**

The MQTT auto-discovery lazy-allocation pattern is replaced with a plain static global for the line buffer, combined with `cMsg` (the global scratch buffer) for the rendered topic:

```cpp
// MQTTstuff.ino
// Raw config line buffer — static global: lines in mqttha.cfg reach ~900 bytes.
// Cannot use cMsg (512 bytes) or stack (too large for ESP8266 4KB CONT stack).
// Topic render target uses the global cMsg scratch buffer (≤200 bytes, fits in CMSG_SIZE).
static char mqttAutoConfigLine[MQTT_CFG_LINE_MAX_LEN];  // 1200 bytes

// In call sites:
char *sLine  = mqttAutoConfigLine;  // raw config line (up to 1200 bytes)
char *sTopic = cMsg;                // rendered topic  (up to 200 bytes, fits in cMsg[512])
```

The `MQTTAutoConfigBuffers` struct is eliminated. The nullable-pointer guard (`if (!acBuf) { ... return; }`) is removed.

**Rule for large feature buffers:**  
If a feature subsystem needs a large working area (≥ ~100 bytes) that lives for the lifetime of the device, declare it as a `static` global in the owning `.ino` file. Do not use `new` / `malloc`, even for "allocate-once, never free" patterns.

This extends ADR-004's core principle ("no dynamic allocation") to explicitly cover large feature-specific working buffers that are only used during specific operations.

## Alternatives Considered

### Alternative 1: Keep lazy `new` allocation (allocate-once, never free)
**Pros:**
- Saves ~1400 bytes of BSS when auto-discovery is never triggered
- Original motivation for the pattern

**Cons:**
- Still fragments the heap at first allocation (unpredictable timing)
- Adds nullable-pointer overhead (`if (!pBuf) { ... }` at every call site)
- Violates ADR-004 with no compensating architectural benefit
- `new` can return `nullptr` on OOM, requiring error handling at every call site

**Why not chosen:** The BSS saving is marginal on a device with 40 KB RAM. The fragmentation risk and code complexity outweigh the benefit.

### Alternative 2: Dynamic allocation with RAII scope management
**Pros:**
- Would limit allocation lifetime to the duration of the auto-discovery run
- 1400 bytes returned to heap after each run

**Cons:**
- Repeated alloc/free causes heap fragmentation (directly what ADR-004 prohibits)
- More complex error-handling at each call site
- Performance overhead during auto-discovery runs

**Why not chosen:** Directly violates ADR-004's core motivation (no fragmentation).

### Alternative 3: Store buffer in PROGMEM / SPI flash
**Pros:**
- Zero RAM cost when not in use

**Cons:**
- Not applicable — working buffers must be writable RAM
- PROGMEM is read-only flash

**Why not chosen:** Not feasible for read-write scratch buffers.

### Alternative 4: Reuse cMsg (512-byte global scratch buffer)
**Pros:**
- Zero additional RAM cost

**Cons:**
- cMsg is only 512 bytes; the autoconfig line buffer needs 1200 bytes (lines reach ~900 bytes)
- cMsg is used throughout the firmware for unrelated operations; sharing it during a multi-step auto-discovery pass would require careful locking

**Partially chosen:** cMsg IS used for the rendered topic (`sTopic`, ≤200 bytes, fits in `cMsg[512]`). The raw config line buffer (`sLine`, up to 1200 bytes) cannot use `cMsg` and must remain a separate static global.

## Consequences

### Positive
- **No heap fragmentation:** `mqttAutoConfigLine` is placed in BSS at link time; no runtime `new` call
- **cMsg used for topic buffer:** `sTopic = cMsg` (200 bytes ≤ 512-byte `cMsg`) — consistent with project scratch-buffer convention
- **Struct eliminated:** `MQTTAutoConfigBuffers` struct removed; only one module-specific static remains (`mqttAutoConfigLine`)
- **Simpler call sites:** OOM guard removed; both pointers are never null
- **Consistent with ADR-004:** No exceptions to the static-allocation rule remain in normal firmware operation
- **Deterministic memory layout:** Full BSS footprint is known at link time

### Negative
- **Always-resident cost:** 1200 bytes of BSS for `mqttAutoConfigLine`, even if auto-discovery is never run
  - **Accepted trade-off:** Deterministic memory layout is worth more on this platform than saving 1200 bytes in a rarely-encountered configuration
- **cMsg shared for sTopic:** `sTopic = cMsg` means callers must not call auto-discovery while `cMsg` is in use for another purpose. Single-threaded ESP8266 loop ensures this is safe.

### Risks & Mitigation
- **Concurrent access:** `mqttAutoConfigLine` and `cMsg` are shared between `doAutoConfigure()` and `doAutoConfigureMsgid()`. The existing `MQTTAutoConfigSessionLock` guard prevents concurrent re-entry; this ADR does not change that behaviour.
- **Buffer overflow:** Line buffer is 1200 bytes; auto-discovery lines that exceed this would silently truncate. This was already true under the lazy-allocation scheme.

## Implementation Patterns

**Feature-specific: use static for oversized buffers, cMsg for small scratch (correct):**
```cpp
// GOOD — large buffer as static global; small scratch via cMsg
static char featureLineBuffer[LARGE_LINE_MAX];  // too large for stack or cMsg

void doFeatureWork() {
  char *sLine  = featureLineBuffer;  // static global — safe
  char *sTopic = cMsg;               // global scratch — fits (≤200 bytes ≤ CMSG_SIZE=512)
  // ... use sLine and sTopic ...
}
```

**Lazy heap allocation (prohibited):**
```cpp
// BAD — heap fragmentation, nullable, violates ADR-004
static MyFeatureBuf* pBuf = nullptr;

MyFeatureBuf* getBuf() {
  if (!pBuf) pBuf = new MyFeatureBuf();  // fragments heap on first call
  return pBuf;
}

void doFeatureWork() {
  MyFeatureBuf* buf = getBuf();
  if (!buf) { /* OOM handling everywhere */ return; }
  // ... use buf ...
}
```

## Related Decisions
- **ADR-004:** Static Buffer Allocation Strategy *(this ADR supersedes it)*
- ADR-001: ESP8266 Platform Selection (memory constraints)
- ADR-009: PROGMEM Usage for String Literals (RAM savings)
- ADR-006: MQTT Integration Pattern (uses static buffers and chunked streaming)
- ADR-030: Heap Memory Monitoring and Emergency Recovery

## References
- MQTT auto-discovery implementation: `src/OTGW-firmware/MQTTstuff.ino` (`mqttAutoConfigLine`, `cMsg` for `sTopic`, `MQTTAutoConfigSessionLock`)
- Heap protection implementation: `src/OTGW-firmware/OTGW-firmware.h` (CSTR macro, heap levels)
- Developer guidelines: `.github/copilot-instructions.md` (Memory Management section)
- PR: copilot/review-codewijzigingen-sinds-laatste-release (commit d514661 — code change; superseding ADR added in follow-up)
