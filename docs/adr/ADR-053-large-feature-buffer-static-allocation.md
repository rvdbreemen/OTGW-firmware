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

**All feature-specific working buffers must be declared as file-static globals or reuse the global `cMsg` scratch buffer — never heap-allocated.**

The MQTT auto-discovery lazy-allocation pattern is replaced by growing the global `cMsg` scratch buffer (`CMSG_SIZE`) from 512 to 1200 bytes (matching `MQTT_CFG_LINE_MAX_LEN`) and using it directly as the line buffer (`sLine`). The rendered topic (`sTopic`) uses a small stack-local buffer:

```cpp
// OTGW-firmware.h
#define CMSG_SIZE 1200  // Large enough for mqttha.cfg lines (max 898 bytes observed)
char cMsg[CMSG_SIZE];   // global scratch buffer

// MQTTstuff.ino — in doAutoConfigure() and doAutoConfigureMsgid()
char *sLine  = cMsg;                  // global scratch (CMSG_SIZE=1200, fits all lines)
char sTopic[MQTT_TOPIC_MAX_LEN];      // stack — 200 bytes, safe on 4KB CONT stack
```

The `MQTTAutoConfigBuffers` struct and the separate `mqttAutoConfigLine[1200]` static are both eliminated. The resulting memory layout:

| Before | After | Delta |
|---|---|---|
| `cMsg[512]` + `mqttAutoConfigLine[1200]` = **1712 bytes BSS** | `cMsg[1200]` = **1200 bytes BSS** | **−512 bytes BSS** |

**Critical constraint:** `expandAndPublishSourceTemplates()` must use `feedWatchDog()` (not `doBackgroundTasks()`) between the 3 source-template variants. Rationale: `topicTemplate` and `msgTemplate` are pointers into `cMsg` (the line buffer); a `doBackgroundTasks()` call can invoke `httpServer.handleClient()` or `handleMQTT()`, which write to `cMsg`, corrupting the template strings before the next variant is rendered. `feedWatchDog()` only feeds the hardware watchdog and blinks the LED — it does not touch `cMsg`.

The `MQTTAutoConfigBuffers` struct is eliminated. The nullable-pointer guard (`if (!acBuf) { ... return; }`) is removed.

**Rule for large feature buffers:**  
If a feature subsystem needs a working area that fits within `CMSG_SIZE`, use `cMsg`. If the area is larger than `CMSG_SIZE`, grow `CMSG_SIZE` to fit and document the reason. Do not use `new` / `malloc`, even for "allocate-once, never free" patterns.

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

### Alternative 4: Keep separate `mqttAutoConfigLine[1200]` static global
**Pros:**
- Simple — no change to `cMsg`, no template-pointer safety concern
- No need to remove `doBackgroundTasks()` from `expandAndPublishSourceTemplates`

**Cons:**
- Total BSS = 512 (`cMsg`) + 1200 (`mqttAutoConfigLine`) = **1712 bytes** — wastes 512 bytes unnecessarily
- Two buffers with overlapping lifetimes in the same call chain is unnecessarily complex

**Why not chosen:** Growing `cMsg` eliminates the duplication and saves 512 bytes BSS with only a minor structural change (`feedWatchDog()` instead of `doBackgroundTasks()` in one function).

## Consequences

### Positive
- **No heap fragmentation:** `cMsg` (1200 bytes) is placed in BSS at link time; no runtime `new` call
- **512 bytes BSS saved:** Replacing `cMsg[512]` + `mqttAutoConfigLine[1200]` with `cMsg[1200]` alone
- **Struct eliminated:** `MQTTAutoConfigBuffers` struct and separate line static removed entirely
- **Simpler call sites:** OOM guard removed; all pointers are never null
- **Consistent with ADR-004:** No exceptions to the static-allocation rule remain in normal firmware operation
- **Deterministic memory layout:** Full BSS footprint is known at link time

### Negative
- **Always-resident cost:** `cMsg[1200]` is always in BSS, even if auto-discovery is never run
  - **Accepted trade-off:** Deterministic memory layout is worth more on this platform than conditional savings
- **`doBackgroundTasks()` removed from inner source-template loop:** `expandAndPublishSourceTemplates()` now uses `feedWatchDog()` only. The device yields slightly less during the 3-variant loop of rare bulk-discovery runs. Since each MQTT publish takes only a few ms, the practical impact is negligible.
- **`CMSG_SIZE` growth affects all `sizeof(cMsg)` call sites:** All existing `snprintf_P(cMsg, sizeof(cMsg), ...)` calls now receive 1200 as the bound. This is safe (more room, not less), and no code assumed `sizeof(cMsg) == 512` explicitly.

### Risks & Mitigation
- **Concurrent access:** `cMsg` is shared between all callers. The existing `MQTTAutoConfigSessionLock` guard prevents concurrent MQTT auto-discovery re-entry. For other `cMsg` users, the single-threaded ESP8266 cooperative model prevents true concurrency; the design requires callers not to hold `cMsg` across `doBackgroundTasks()` calls.
- **Buffer overflow:** Line buffer capacity is now `CMSG_SIZE=1200` bytes; auto-discovery lines that exceed this would silently truncate. This was already true under the previous scheme.
- **Template corruption:** Mitigated by using `feedWatchDog()` (not `doBackgroundTasks()`) in `expandAndPublishSourceTemplates()` — see Decision section.

## Implementation Patterns

**Canonical pattern — use `cMsg` for all scratch work up to `CMSG_SIZE` (correct):**
```cpp
// GOOD — use cMsg for raw line buffer (grown to CMSG_SIZE=1200);
//         stack buffer for small rendered output (200 bytes ≤ 4KB CONT stack)
char *sLine  = cMsg;                  // global scratch — safe, CMSG_SIZE=1200
char sTopic[MQTT_TOPIC_MAX_LEN];      // stack — 200 bytes, safe on CONT stack

// IMPORTANT: do NOT call doBackgroundTasks() while cMsg holds data you still need.
// Use feedWatchDog() instead (does not touch cMsg).
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
- MQTT auto-discovery implementation: `src/OTGW-firmware/MQTTstuff.ino` (`cMsg` as `sLine`, stack `sTopic`, `MQTTAutoConfigSessionLock`)
- Global scratch buffer: `src/OTGW-firmware/OTGW-firmware.h` (`CMSG_SIZE`, `cMsg[CMSG_SIZE]`)
- Developer guidelines: `.github/copilot-instructions.md` (Memory Management section)
- PR: copilot/review-codewijzigingen-sinds-laatste-release (code change and ADR follow-up)
