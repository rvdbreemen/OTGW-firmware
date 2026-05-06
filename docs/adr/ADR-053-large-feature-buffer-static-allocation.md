# ADR-053: Large Feature Buffer Static Allocation

## Status

Accepted, 2026-03-21. Supersedes: ADR-004 (Static Buffer Allocation Strategy).

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

After an iterative analysis of all buffer uses in the codebase (see Alternatives Considered), the optimal minimum-memory design was determined to be two named globals: `cMsg[512]` for general scratch work and `sLine[1200]` dedicated to the MQTT autoconfig line buffer.

## Decision

**All feature-specific working buffers must be declared as global arrays — never heap-allocated, never local static, never local stack. When a feature needs a large buffer, add a named purpose-specific global.**

The MQTT auto-discovery lazy-allocation pattern is replaced by two explicitly-named global buffers declared in `OTGW-firmware.h`:

```cpp
// OTGW-firmware.h
#define CMSG_SIZE  512   // General-purpose scratch buffer (webhook, REST API, JSON, MQTT topic).
                         // All known users need ≤512 bytes.
#define SLINE_SIZE 1200  // MQTT autoconfig line buffer.  mqttha.cfg lines reach ~900 bytes max.
                         // Exclusively owned by MQTTstuff.ino; guarded by mqttAutoConfigInProgress.
char cMsg[CMSG_SIZE];    // global general-purpose scratch buffer
char sLine[SLINE_SIZE];  // MQTT autoconfig line scratch (MQTTstuff.ino, guarded by mqttAutoConfigInProgress)

// MQTTstuff.ino — in doAutoConfigure() and doAutoConfigureMsgid()
char *sTopic = cMsg;     // global scratch reused for rendered topic (≤200 bytes, fits in CMSG_SIZE)
                         // Safe: topicTemplate/msgTemplate point INTO sLine, not cMsg.
```

The `MQTTAutoConfigBuffers` struct and the `mqttAutoConfigLine[1200]` file-scope static are eliminated. The resulting memory layout:

| Design | BSS for these buffers | Notes |
|---|---|---|
| Previous: `cMsg[512]` + heap `MQTTAutoConfigBuffers` (1400B) | **512 bytes BSS** + heap | Heap fragmentation |
| Previous: `cMsg[512]` + `mqttAutoConfigLine[1200]` + stack `sTopic[200]` | **1712 bytes BSS** | Local static + stack |
| Enlarged single buffer: `cMsg[1200]` + stack `sTopic[200]` | **1200 bytes BSS** | Stack variable |
| **Final (this ADR): `cMsg[512]` + `sLine[1200]` + `cMsg` as sTopic** | **1712 bytes BSS** | No stack/static, clear names |

**Why `cMsg` (not a stack variable) is the right choice for `sTopic`:**  
With `sLine` holding the raw config line, `topicTemplate` and `msgTemplate` are pointers INTO `sLine`. Rendering `sTopic` into `cMsg` is safe because `cMsg` and `sLine` are disjoint globals. No stack allocation is needed, and no risk of corrupting the templates.

**Why NOT using a single `cMsg[1200]`:**  
Growing `cMsg` to 1200 bytes would silently widen the bounds for all `snprintf_P(cMsg, sizeof(cMsg), ...)` call sites — webhook, REST API, JSON helpers — which need ≤512 bytes. A 1200-byte `cMsg` gives them a bound they will never use, wasting 688 bytes of BSS permanently. A named `sLine[1200]` makes the MQTT-specific usage explicit and keeps `cMsg` bounded at its correct size.

**Critical constraint:** `expandAndPublishSourceTemplates()` and the per-line publish loop must use `feedWatchDog()` (not `doBackgroundTasks()`) when `cMsg` is live as `sTopic`. Rationale: `doBackgroundTasks()` routes HTTP/MQTT callbacks that write to `cMsg`, which would corrupt the rendered topic. `feedWatchDog()` feeds the hardware watchdog only — it does not touch `cMsg` or `sLine`.

The `MQTTAutoConfigBuffers` struct is eliminated. The nullable-pointer guard (`if (!acBuf) { ... return; }`) is removed.

**Rules for feature buffers:**  
- Use `cMsg` for all general scratch work (≤512 bytes).  
- For scratch work >512 bytes, add a named global with a descriptive size constant.  
- Never use `new` / `malloc`, even for "allocate-once, never free" patterns.  
- Never use local static buffers (hidden persistent state).
- Never use local stack buffers for MQTT autoconfig workspace (defeats the purpose of explicit global ownership).

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

### Alternative 4: Two named global scratch buffers (`cMsg[512]` + `sLine[1200]`) — **chosen**
**Pros:**
- `cMsg` stays bounded at its original 512 bytes for all other callers — no silent bound changes
- Named `sLine` buffer makes MQTT-specific large-buffer usage visible at a glance
- `cMsg` can be reused as `sTopic` (safe because templates point into `sLine`, not `cMsg`)
- No stack or local static buffers anywhere in the MQTT autoconfig path
- Both buffers are explicitly guarded: `mqttAutoConfigInProgress` gates all access to `sLine` and to `cMsg`-as-sTopic

**Cons:**
- 1712 bytes BSS (512 bytes more than the single `cMsg[1200]` design)
- `sLine` is always resident even if MQTT autoconfig is never run

**Why chosen:** Explicit ownership is more important than saving 512 bytes. `cMsg` keeps its original semantic (general-purpose, ≤512 bytes); `sLine` clearly names the MQTT-specific large buffer. The 512-byte cost is the same as having both the old `mqttAutoConfigLine` file-scope static AND `cMsg[512]`, so there is no regression vs. the pre-PR state.

### Alternative 5: Single enlarged `cMsg[1200]` with `sTopic` as local stack variable
**Pros:**
- Only 1200 bytes BSS (−512 vs. two-global design)
- One fewer global to declare and document

**Cons:**
- `sizeof(cMsg)` silently returns 1200 everywhere: webhook, REST API, JSON helpers that write ≤512 bytes will receive a bound of 1200 — technically safe but misleading
- `sTopic` becomes a local stack variable (200 bytes) — still a "local buffer" which is the pattern we are trying to eliminate
- The purpose of the large `cMsg` is not obvious to future maintainers; comment must explain why a general-purpose scratch buffer is 1200 bytes

**Why not chosen:** The 200-byte stack variable re-introduces the "local buffer" anti-pattern, and the 512-byte saving is not worth the semantic confusion of a 1200-byte general-purpose scratch buffer.

### Alternative 6: Dedicated `sTopicBuf[MQTT_TOPIC_MAX_LEN]` global for sTopic
**Pros:**
- All buffers are named: `cMsg` (general scratch), `sLine` (config line), `sTopicBuf` (MQTT topic)
- Zero risk of aliasing between any pair of buffers

**Cons:**
- Adds 200 bytes BSS: total 1712 + 200 = **1912 bytes BSS**
- `sTopicBuf` is only used during MQTT autoconfig topic rendering — wasteful for the rest of firmware operation
- `cMsg` already serves the same purpose safely (given `sLine` separation)

**Why not chosen:** 200 extra bytes for a third large global when `cMsg` already does the job safely with the two-buffer design.

## Consequences

### Positive
- **No heap fragmentation:** `cMsg` and `sLine` are placed in BSS at link time; no runtime `new` call
- **No large local work buffers:** No large stack or local static workspace buffers in the MQTT autoconfig path; primary work buffers (`cMsg`, `sLine`) are global, and only small, short-lived local helpers are permitted
- **Struct eliminated:** `MQTTAutoConfigBuffers` struct removed; no nullable-pointer boilerplate
- **Simpler call sites:** OOM guard removed; `cMsg` and `sLine` are never null
- **Consistent with ADR-004:** No exceptions to the static-allocation rule remain in normal firmware operation
- **Deterministic memory layout:** Full BSS footprint is known at link time
- **Clear buffer ownership:** `cMsg` for general scratch (≤512 bytes), `sLine` for MQTT autoconfig lines (≤1200 bytes)

### Negative
- **Always-resident cost:** `sLine[1200]` is always in BSS, even if auto-discovery is never run
  - **Accepted trade-off:** Deterministic memory layout is worth more on this platform than conditional savings
  - The old design also had `mqttAutoConfigLine[1200]` always resident — no regression
- **`doBackgroundTasks()` removed from inner per-line loop:** `feedWatchDog()` is the only yield used during autoconfig. The device yields slightly less during bulk-discovery runs; each MQTT publish takes only a few ms, so the practical impact is negligible.
- **BSS slightly larger than single-buffer design:** 1712 bytes vs. 1200 bytes for the single `cMsg[1200]` alternative. Accepted because explicit named buffers prevent semantic confusion.

### Risks & Mitigation
- **Concurrent access — `sLine`:** `sLine` is guarded by `mqttAutoConfigInProgress` (via `MQTTAutoConfigSessionLock`). Any code path that tries to acquire the lock while it is held will see the guard and return early.
- **Concurrent access — `cMsg` as sTopic:** `cMsg` is held as `sTopic` only during MQTT autoconfig, during which `feedWatchDog()` (not `doBackgroundTasks()`) is the only yield. No HTTP/MQTT callback can overwrite `cMsg` during that window.
- **Buffer overflow during MQTT line read:** Line buffer capacity is `SLINE_SIZE=1200` bytes; lines exceeding this would silently truncate at `SLINE_SIZE − 1`. The maximum observed line in `mqttha.cfg` is ~900 bytes, so 1200 bytes provides adequate headroom.
- **`cMsg` (sTopic) bound:** `sTopic` uses `cMsg[512]`; rendered MQTT topics are bounded to `MQTT_TOPIC_MAX_LEN=200` bytes by `renderTemplateToBuffer()` and `replaceAll()`. A 200-byte topic fits easily in `cMsg[512]`.

## Implementation Patterns

**Final pattern — two named globals, `cMsg` as sTopic pointer (correct):**
```cpp
// OTGW-firmware.h
#define CMSG_SIZE  512   // General-purpose scratch buffer.  All users need ≤512 bytes.
#define SLINE_SIZE 1200  // MQTT autoconfig line buffer.  Lines reach ~900 bytes max.
char cMsg[CMSG_SIZE];    // global general-purpose scratch
char sLine[SLINE_SIZE];  // MQTT autoconfig line scratch (MQTTstuff.ino, guarded)

// MQTTstuff.ino — doAutoConfigure() and doAutoConfigureMsgid()
// Acquire lock first (gates sLine and cMsg-as-sTopic):
MQTTAutoConfigSessionLock sessionLock;
if (!sessionLock.locked) { return; }

char *sTopic = cMsg;  // global scratch reused as rendered topic (≤200 bytes, fits in CMSG_SIZE)

// Read config line into sLine (the dedicated global, guarded):
size_t len = fh.readBytesUntil('\n', sLine, SLINE_SIZE - 1);
sLine[len] = '\0';
parseAutoConfigLine(sLine, ';', &lineView);  // topicTemplate/msgTemplate point into sLine

// Render topic into cMsg (safe because cMsg and sLine are disjoint):
renderTemplateToBuffer(lineView.topicTemplate, sTopic, MQTT_TOPIC_MAX_LEN, &renderCtx);

// IMPORTANT: use feedWatchDog() (not doBackgroundTasks()) while cMsg is live as sTopic.
// doBackgroundTasks() routes HTTP/MQTT callbacks that write to cMsg.
feedWatchDog();
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
- MQTT auto-discovery implementation: `src/OTGW-firmware/MQTTstuff.ino` (`sLine` as config line buffer, `cMsg` as sTopic, `MQTTAutoConfigSessionLock`)
- Global scratch buffers: `src/OTGW-firmware/OTGW-firmware.h` (`CMSG_SIZE=512`, `SLINE_SIZE=1200`, `cMsg[CMSG_SIZE]`, `sLine[SLINE_SIZE]`)
- Developer guidelines: `.github/copilot-instructions.md` (Memory Management section)
- PR: copilot/review-codewijzigingen-sinds-laatste-release (code change and ADR follow-up)
