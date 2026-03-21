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

After an iterative analysis of all buffer uses in the codebase (see Alternatives Considered), the optimal minimum-memory design was determined to be a single enlarged `cMsg[1200]` global that serves both as the general scratch buffer and as the MQTT autoconfig line workspace.

## Decision

**All feature-specific working buffers must be declared as global or file-scope static arrays — never heap-allocated. When a feature needs a buffer larger than the default general scratch size, the general scratch buffer is enlarged to accommodate it rather than adding a second large global.**

The MQTT auto-discovery lazy-allocation pattern is replaced by a single enlarged global scratch buffer declared in `OTGW-firmware.h`:

```cpp
// OTGW-firmware.h
#define CMSG_SIZE  1200  // General-purpose scratch buffer + MQTT autoconfig line workspace.
                         // Sized to mqttha.cfg max observed line (~900 bytes) with headroom.
                         // All other users (webhook, REST API, JSON, MQTT topic) need ≤512 bytes.
char cMsg[CMSG_SIZE];    // global scratch buffer

// MQTTstuff.ino — in doAutoConfigure() and doAutoConfigureMsgid()
char sTopic[MQTT_TOPIC_MAX_LEN];  // local stack variable (200 bytes) — separate from cMsg
                                   // because topicTemplate/msgTemplate point INTO cMsg
```

The `MQTTAutoConfigBuffers` struct, the `sLine[SLINE_SIZE]` global, and the `mqttAutoConfigLine[1200]` file-scope static are all eliminated. The resulting memory layout:

| Design | BSS for these buffers | Notes |
|---|---|---|
| Previous: `cMsg[512]` + `mqttAutoConfigLine[1200]` + stack `sTopic[200]` | **1712 bytes BSS** | Two globals |
| Multi-buffer: `cMsg[512]` + `sLine[1200]` + `cMsg` as sTopic | **1712 bytes BSS** | Two globals, same total |
| **Final (this ADR): `cMsg[1200]` + stack `sTopic[200]`** | **1200 bytes BSS** | Single global, −512 bytes |

**Why `sTopic` must be a local stack variable (not `cMsg`):**  
`sTopic` holds the *rendered* topic (≤200 bytes). `topicTemplate` and `msgTemplate` are pointers INTO `cMsg` (the raw config line). Rendering `sTopic` directly into `cMsg` would overwrite the template that the message rendering still needs. A 200-byte stack allocation is safe on the ESP8266's 4 KB CONT-stack.

**Critical constraint:** `expandAndPublishSourceTemplates()` must use `feedWatchDog()` (not `doBackgroundTasks()`) between the 3 source-template variants. Rationale: `topicTemplate` and `msgTemplate` are pointers into `cMsg`; `doBackgroundTasks()` routes HTTP/MQTT callbacks that write to `cMsg`, corrupting those pointers. `feedWatchDog()` feeds the hardware watchdog only — it does not touch `cMsg`.

The `MQTTAutoConfigBuffers` struct is eliminated. The nullable-pointer guard (`if (!acBuf) { ... return; }`) is removed.

**Rule for feature buffers:**  
- Use `cMsg` for all scratch work.  
- If a feature needs more space than the current `CMSG_SIZE`, increase `CMSG_SIZE` with a comment explaining the new minimum.  
- Never use `new` / `malloc`, even for "allocate-once, never free" patterns.  
- Never use local static buffers (persists across calls, hidden state).

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

### Alternative 4: Two named global scratch buffers (`cMsg[512]` + `sLine[1200]`)
**Pros:**
- `cMsg` stays bounded at 512 bytes for all other callers
- Named `sLine` buffer makes MQTT-specific usage visible at a glance

**Cons:**
- 1712 bytes BSS (512 bytes more than the chosen design)
- Two large globals always resident, even though `sLine` is only used during MQTT autoconfig
- `cMsg` at 512 bytes can still be used as `sTopic` (rendering into `cMsg` is safe with this design)

**Why not chosen:** 512 bytes more BSS for a naming benefit that is better achieved with a comment on `CMSG_SIZE`. All `cMsg` users (REST API, webhook, JSON helpers) need ≤512 bytes; growing `cMsg` to 1200 bytes adds headroom they never use, but costs nothing at runtime.

### Alternative 5: Dedicated `sTopicBuf[MQTT_TOPIC_MAX_LEN]` global for sTopic
**Pros:**
- All buffers are named: `cMsg` (general scratch + line), `sTopicBuf` (MQTT topic)
- Zero risk of aliasing between the line buffer and the topic buffer

**Cons:**
- Adds 200 bytes BSS that can be avoided by a 200-byte stack allocation
- `sTopicBuf` is only non-null during MQTT autoconfig iterations — wasteful for the rest of firmware operation
- The ESP8266 CONT-stack is 4 KB; 200 bytes is a trivially small stack frame

**Why not chosen:** A 200-byte stack variable during autoconfig (a rare, brief operation) is better than 200 bytes of always-resident BSS.

## Consequences

### Positive
- **No heap fragmentation:** `cMsg` is placed in BSS at link time; no runtime `new` call
- **Minimum BSS:** 1200 bytes for this buffer region (512 bytes less than the two-global design)
- **Struct eliminated:** `MQTTAutoConfigBuffers` struct removed; no nullable-pointer boilerplate
- **Simpler call sites:** OOM guard removed; `cMsg` is never null
- **Consistent with ADR-004:** No exceptions to the static-allocation rule remain in normal firmware operation
- **Deterministic memory layout:** Full BSS footprint is known at link time

### Negative
- **Always-resident cost:** `cMsg[1200]` is always in BSS, even if auto-discovery is never run
  - **Accepted trade-off:** Deterministic memory layout is worth more on this platform than conditional savings
- **`doBackgroundTasks()` removed from inner source-template loop:** `expandAndPublishSourceTemplates()` uses `feedWatchDog()` only between per-source publishes. The device yields slightly less during the 3-variant loop of rare bulk-discovery runs; each MQTT publish takes only a few ms, so the practical impact is negligible.
- **`cMsg` size increased:** From 512 to 1200 bytes. All `snprintf_P(cMsg, sizeof(cMsg), ...)` call sites will silently receive 1200 as the bound instead of 512. Since these callers only write ≤512 bytes, the larger bound is always safe.

### Risks & Mitigation
- **Concurrent access:** `cMsg` is shared globally. The existing `MQTTAutoConfigSessionLock` guard prevents concurrent MQTT auto-discovery re-entry. For other `cMsg` users, the single-threaded ESP8266 cooperative model prevents true concurrency; callers must not hold `cMsg` across `doBackgroundTasks()` calls.
- **Buffer overflow during MQTT line read:** Line buffer capacity is `CMSG_SIZE=1200` bytes; auto-discovery lines that exceed this would silently truncate at `CMSG_SIZE − 1`. The maximum observed line in `mqttha.cfg` is ~900 bytes, so 1200 bytes provides adequate headroom.
- **sTopic/cMsg aliasing:** `sTopic` is a local stack variable (200 bytes); it is distinct from `cMsg`. `topicTemplate`/`msgTemplate` point into `cMsg`; rendering into `sTopic` does not corrupt them. `feedWatchDog()` (not `doBackgroundTasks()`) is used inside `expandAndPublishSourceTemplates()` to ensure `cMsg` is not overwritten between `renderTemplateToBuffer()` and `sendMQTTTemplateStreaming()` within the same iteration.

## Implementation Patterns

**Final pattern — single enlarged `cMsg` (correct):**
```cpp
// OTGW-firmware.h
#define CMSG_SIZE  1200  // General-purpose scratch buffer + MQTT autoconfig line workspace.
                         // Sized to mqttha.cfg max observed line (~900 bytes) with headroom.
char cMsg[CMSG_SIZE];    // global scratch buffer (only one large buffer needed)

// MQTTstuff.ino — doAutoConfigure() and doAutoConfigureMsgid()
char sTopic[MQTT_TOPIC_MAX_LEN];  // 200-byte stack variable (separate from cMsg line buffer)

// Read config line into cMsg:
size_t len = fh.readBytesUntil('\n', cMsg, CMSG_SIZE - 1);
cMsg[len] = '\0';
parseAutoConfigLine(cMsg, ';', &lineView);  // topicTemplate/msgTemplate point into cMsg

// Render topic into the SEPARATE stack buffer:
renderTemplateToBuffer(lineView.topicTemplate, sTopic, MQTT_TOPIC_MAX_LEN, &renderCtx);

// IMPORTANT: do NOT call doBackgroundTasks() within expandAndPublishSourceTemplates()
// while cMsg holds the active config line.  Use feedWatchDog() instead.
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

**Local static buffer (prohibited):**
```cpp
// BAD — hidden persistent state, violates no-local-static rule
void doFeatureWork() {
  static char scratch[512];  // persists across calls; invisible to other modules
  // ... use scratch ...
}

// GOOD — use cMsg instead
void doFeatureWork() {
  snprintf_P(cMsg, sizeof(cMsg), PSTR("..."), ...);
  // ... use cMsg ...
}
```

## Related Decisions
- **ADR-004:** Static Buffer Allocation Strategy *(this ADR supersedes it)*
- ADR-001: ESP8266 Platform Selection (memory constraints)
- ADR-009: PROGMEM Usage for String Literals (RAM savings)
- ADR-006: MQTT Integration Pattern (uses static buffers and chunked streaming)
- ADR-030: Heap Memory Monitoring and Emergency Recovery

## References
- MQTT auto-discovery implementation: `src/OTGW-firmware/MQTTstuff.ino` (`cMsg` as config line buffer, `sTopic` as local stack, `MQTTAutoConfigSessionLock`)
- Global scratch buffer: `src/OTGW-firmware/OTGW-firmware.h` (`CMSG_SIZE=1200`, `cMsg[CMSG_SIZE]`)
- Developer guidelines: `.github/copilot-instructions.md` (Memory Management section)
- PR: copilot/review-codewijzigingen-sinds-laatste-release (code change and ADR follow-up)
