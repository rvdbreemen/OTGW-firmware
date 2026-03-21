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

**All feature-specific working buffers must be declared as global or file-scope static arrays — never heap-allocated. Large feature buffers that exceed `CMSG_SIZE` get their own named global; `cMsg` handles small scratch work (≤`CMSG_SIZE`).**

The MQTT auto-discovery lazy-allocation pattern is replaced by two global scratch buffers declared in `OTGW-firmware.h`:

```cpp
// OTGW-firmware.h
#define CMSG_SIZE  512   // General-purpose scratch (webhook, REST API, JSON formatting, etc.)
#define SLINE_SIZE 1200  // MQTT autoconfig line buffer; mqttha.cfg lines reach ~900 bytes
char cMsg[CMSG_SIZE];    // global scratch buffer
char sLine[SLINE_SIZE];  // MQTT autoconfig line scratch (MQTTstuff.ino)

// MQTTstuff.ino — in doAutoConfigure() and doAutoConfigureMsgid()
char *sTopic = cMsg;     // reuse global scratch for rendered topic (≤200 bytes, fits in CMSG_SIZE)
// sLine is accessed directly as the global array (no local pointer needed)
```

The `MQTTAutoConfigBuffers` struct and the `mqttAutoConfigLine[1200]` file-scope static are both eliminated. The resulting memory layout:

| Design | BSS for these buffers |
|---|---|
| Previous: `cMsg[512]` + `mqttAutoConfigLine[1200]` + stack `sTopic[200]` | **1712 bytes BSS** |
| Interim (grew cMsg): `cMsg[1200]` + stack `sTopic[200]` | **1200 bytes BSS** |
| **Final (this ADR): `cMsg[512]` + `sLine[1200]` + `cMsg` as sTopic** | **1712 bytes BSS** |

**Why the final design over the "grew cMsg" interim:**
- `cMsg` reverts to its natural 512-byte general-purpose role; no unexpected surprise for other call sites
- `sLine` has a clear, named purpose — cannot be accidentally confused with the general scratch buffer
- The multi-buffer approach makes buffer ownership obvious at a glance in the code
- All buffers are global (no local statics, no stack allocations for large data); the user's preference for "multiple named global scratch buffers" is fully expressed

**Using `cMsg` for `sTopic` (rendered MQTT topic, ≤200 bytes):**  
`cMsg` is safe for `sTopic` because:
1. `renderTemplateToBuffer()` and `replaceAll()` write only to their `dest` parameter — no internal `cMsg` usage
2. `sendMQTTTemplateStreaming()` uses the PubSubClient streaming API — no internal `cMsg` usage
3. `doBackgroundTasks()` (called after each publish) may overwrite `cMsg`, but `cMsg` is re-rendered from `sLine` at the start of the next iteration, so the overwrite is harmless

**Critical constraint:** `expandAndPublishSourceTemplates()` must use `feedWatchDog()` (not `doBackgroundTasks()`) between the 3 source-template variants. Rationale: `topicTemplate` and `msgTemplate` are pointers into `sLine` (the global line buffer); those pointers are safe from `doBackgroundTasks()` corruption — only `cMsg` is at risk. However, `cMsg` is used as `renderedTopic` (the per-variant rendered topic) within the inner loop. `doBackgroundTasks()` can overwrite `cMsg` between `renderTemplateToBuffer()` and `sendMQTTTemplateStreaming()` within the *same* iteration. `feedWatchDog()` does not touch `cMsg`.

The `MQTTAutoConfigBuffers` struct is eliminated. The nullable-pointer guard (`if (!acBuf) { ... return; }`) is removed.

**Rule for feature buffers:**  
- If the data fits in `CMSG_SIZE` (512 bytes), use `cMsg`.  
- If the data exceeds `CMSG_SIZE`, declare a dedicated named global (e.g., `sLine[SLINE_SIZE]`) and document the reason.  
- Never use `new` / `malloc`, even for "allocate-once, never free" patterns.

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

### Alternative 4: Grow `cMsg` to 1200 bytes; eliminate `sLine` global (interim design)
**Pros:**
- Minimum BSS (1200 bytes vs 1712 bytes for separate globals)
- Single buffer for both line reading and topic rendering

**Cons:**
- `cMsg` is used by all modules (REST API, webhook, JSON helpers, MQTT, …); growing it from 512 to 1200 bytes silently inflates memory for every `sizeof(cMsg)` call site
- Other callers of `cMsg` only need ≤512 bytes; the extra 688 bytes are always wasted by code that has nothing to do with MQTT autoconfig
- The `topicTemplate`/`msgTemplate` pointers live inside `cMsg`, so `doBackgroundTasks()` cannot be called while processing source templates — subtle constraint, easy to violate in future refactors
- `cMsg`'s dual role (general scratch AND large MQTT line buffer) is invisible from call sites

**Why not chosen (final decision):** The 512-byte BSS saving comes at the cost of a hidden semantic tie between `cMsg` and the MQTT autoconfig subsystem. Explicit naming (`sLine`) is better than saving 512 bytes.

### Alternative 5: Dedicated `sTopicBuf[MQTT_TOPIC_MAX_LEN]` global for sTopic
**Pros:**
- All three buffers are named: `cMsg` (general scratch), `sLine` (MQTT line), `sTopicBuf` (MQTT topic)
- Zero risk of cMsg being dual-purpose

**Cons:**
- Adds 200 bytes BSS that can be avoided by reusing `cMsg` (sTopic is ≤200 bytes, fits in `cMsg[512]`)
- `sTopicBuf` is only non-null during MQTT autoconfig iterations — wasteful for the rest of firmware operation

**Why not chosen:** Using `cMsg` for sTopic is safe (confirmed analysis: neither `renderTemplateToBuffer()` nor `sendMQTTTemplateStreaming()` write to `cMsg` internally) and saves 200 bytes BSS with no added risk.

## Consequences

### Positive
- **No heap fragmentation:** `cMsg` and `sLine` are placed in BSS at link time; no runtime `new` call
- **Clear buffer ownership:** `cMsg` = general scratch (512 bytes); `sLine` = MQTT autoconfig lines (1200 bytes); roles are explicit in code
- **Struct eliminated:** `MQTTAutoConfigBuffers` struct removed; no nullable-pointer boilerplate
- **Simpler call sites:** OOM guard removed; all pointers are never null
- **Consistent with ADR-004:** No exceptions to the static-allocation rule remain in normal firmware operation
- **Deterministic memory layout:** Full BSS footprint is known at link time
- **`cMsg` unchanged for other callers:** All existing `snprintf_P(cMsg, sizeof(cMsg), ...)` calls continue to receive 512 as the bound — no silent behaviour change

### Negative
- **Always-resident cost:** `sLine[1200]` is always in BSS, even if auto-discovery is never run
  - **Accepted trade-off:** Deterministic memory layout is worth more on this platform than conditional savings
- **+512 bytes BSS vs "grew cMsg" interim:** The multi-buffer design uses 1712 bytes where the interim cMsg[1200] approach used 1200 bytes. This 512-byte premium pays for clearer semantics and a `cMsg` that stays bounded at 512 bytes for all other modules.
- **`doBackgroundTasks()` removed from inner source-template loop:** `expandAndPublishSourceTemplates()` uses `feedWatchDog()` only between per-source publishes. The device yields slightly less during the 3-variant loop of rare bulk-discovery runs; each MQTT publish takes only a few ms, so the practical impact is negligible.

### Risks & Mitigation
- **Concurrent access:** `cMsg` and `sLine` are shared globally. The existing `MQTTAutoConfigSessionLock` guard prevents concurrent MQTT auto-discovery re-entry. For other `cMsg` users, the single-threaded ESP8266 cooperative model prevents true concurrency; callers must not hold `cMsg` across `doBackgroundTasks()` calls.
- **Buffer overflow:** Line buffer capacity is `SLINE_SIZE=1200` bytes; auto-discovery lines that exceed this would silently truncate at `MQTT_CFG_LINE_MAX_LEN − 1`.
- **cMsg/sTopic aliasing:** `cMsg` is used as `sTopic` during MQTT autoconfig iterations. `feedWatchDog()` (not `doBackgroundTasks()`) is used inside `expandAndPublishSourceTemplates()` to ensure `cMsg` is not overwritten between `renderTemplateToBuffer()` and `sendMQTTTemplateStreaming()` within the same iteration.

## Implementation Patterns

**Final pattern — two named global scratch buffers (correct):**
```cpp
// OTGW-firmware.h
#define CMSG_SIZE  512   // general scratch
#define SLINE_SIZE 1200  // MQTT autoconfig line buffer
char cMsg[CMSG_SIZE];
char sLine[SLINE_SIZE];

// MQTTstuff.ino — doAutoConfigure() and doAutoConfigureMsgid()
char *sTopic = cMsg;    // global scratch reused as topic (≤200 bytes, fits in CMSG_SIZE)
// 'sLine' accesses the global directly — no local alias needed

// IMPORTANT: do NOT call doBackgroundTasks() within expandAndPublishSourceTemplates()
// while cMsg holds renderedTopic between renderTemplateToBuffer() and
// sendMQTTTemplateStreaming() within a single variant.  Use feedWatchDog() instead.
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
- MQTT auto-discovery implementation: `src/OTGW-firmware/MQTTstuff.ino` (`sLine` global as config line, `cMsg` as sTopic, `MQTTAutoConfigSessionLock`)
- Global scratch buffers: `src/OTGW-firmware/OTGW-firmware.h` (`CMSG_SIZE`/`SLINE_SIZE`, `cMsg[CMSG_SIZE]`, `sLine[SLINE_SIZE]`)
- Developer guidelines: `.github/copilot-instructions.md` (Memory Management section)
- PR: copilot/review-codewijzigingen-sinds-laatste-release (code change and ADR follow-up)
