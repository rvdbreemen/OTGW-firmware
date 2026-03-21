---
# METADATA
Document Title: Critical Code Review & Refactoring Analysis
Review Date: 2026-03-19 06:36:25 UTC
Branch Reviewed: copilot/review-refactoring-gains
Reviewer: GitHub Copilot Advanced Agent
Document Type: Critical Review — No code changes made
Status: AWAITING DECISION
---

# Critical Code Review — OTGW-firmware

**Purpose:** Identify technical debt, assess refactoring gain, and present findings for owner decision.
**Scope:** All 20 source files in `src/OTGW-firmware/` (~10,500 lines of production code).
**Methodology:** Static analysis + pattern matching. No code changes are made in this PR.

---

## Executive Summary

The firmware is **functionally solid and well-structured in its core design choices** (ADR-compliant, modular, correct PROGMEM discipline in critical paths). However, it carries **three concentrations of technical debt** that have real maintenance and correctness cost, and **one security defect** worth addressing.

| Finding | Severity | Effort | Risk of fixing |
|---------|----------|--------|---------------|
| F1: Duplicate `jsonEscape` — different behaviour | 🔴 Defect | 30 min | Low |
| F2: `print_*` boilerplate — 27 functions with identical skeletons | 🟠 Debt | 4–6 h | Medium |
| F3: `String` class in FSexplorer hot paths | 🟠 Debt | 2–3 h | Low–Medium |
| F4: `processOT()` is 327 lines | 🟡 Maintainability | 2–3 h | Medium |
| F5: Per-module debug macro duplication | 🟡 Style debt | 1 h | Low |
| F6: Dead code in `processOT()` | 🟢 Cleanup | 15 min | Trivial |
| F7: `String` class in upgrade/flash paths | 🟢 Low-risk debt | 1–2 h | Low |

---

## F1 — Duplicate `jsonEscape` with Different Behaviour (🔴 Defect)

**Files:** `OTGW-Core.ino:3958` and `jsonStuff.ino:14`

Two implementations exist that **behave differently on control characters**:

| | `jsonStuff.ino::escapeJsonStringTo()` | `OTGW-Core.ino::jsonEscape()` |
|---|---|---|
| `"` | `\"` ✅ | `\"` ✅ |
| `\\` | `\\` ✅ | `\\` ✅ |
| `\n` | `\n` (JSON escape) ✅ | ` ` (space) ⚠️ |
| `\t \r \b \f` | proper JSON escapes ✅ | ` ` (space) ⚠️ |
| `< 0x20` | `\uXXXX` ✅ | ` ` (space) ⚠️ |

`OTGW-Core.ino::jsonEscape()` is used only in `fwupgradedone()` (lines 4015–4016 and 4054) to escape the PIC filename and error string into JSON payloads served to the browser. If either string contains a newline or tab, the `jsonEscape()` version silently corrupts the JSON instead of properly encoding it.

**Recommendation:** Replace `OTGW-Core.ino::jsonEscape()` with a call to `escapeJsonStringTo()` from `jsonStuff.ino`. The declaration is already in `OTGW-firmware.h:120`. This is a three-line change (remove the private function, update the three call sites).

---

## F2 — `print_*` Boilerplate: 27 Functions, ~1 Identical Skeleton (🟠 Debt)

**File:** `OTGW-Core.ino`, lines 1435–2060 and scattered to ~2800

There are **27 `print_*` functions** and **111 combined calls** to `sendMQTTData` / `publishToSourceTopic` in `OTGW-Core.ino`. The overwhelming majority follow this skeleton:

```cpp
void print_TYPE(TYP& value) {
  // 1. Convert OTdata fields to string
  char _msg[15] {0};
  CONVERT_FUNCTION(OTdata.FIELD(), _msg, ...);

  // 2. Log
  AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);

  // 3. Publish to MQTT if valid
  if (is_value_valid(OTdata, OTlookupitem)) {
    const char* topic = messageIDToString(...);
    sendMQTTData(topic, _msg);
    publishToSourceTopic(topic, _msg, OTdata.rsptype);
    value = _value;
  }
}
```

The skeleton is identical across `print_f88`, `print_s16`, `print_u16`, `print_flag8`, `print_flag8flag8`, `print_flag8u8`, `print_u8u8`, and many more. Only the *conversion step* and the *field suffix* differ.

**Concrete problem — `print_flag8` omits `publishToSourceTopic`:**
```cpp
// print_f88:
sendMQTTData(topic, _msg);
publishToSourceTopic(topic, _msg, OTdata.rsptype);  // ✅ present

// print_flag8:
sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
// publishToSourceTopic missing ⚠️
```
This means flag8 values never reach source-specific topics. This is an observable bug — it was silently introduced because adding source-topic publishing requires copy-pasting a line into each function separately.

**Recommendation options (pick one):**

| Option | Description | Gain | Risk |
|--------|-------------|------|------|
| A | Add the missing `publishToSourceTopic` calls to the functions that omit them | Low effort, fixes the bug | Low (targeted fix) |
| B | Extract a shared `publishScalarOTValue(const char* topic, const char* msg)` helper that always does both calls, and update all print functions to use it | Medium effort, prevents future drift | Medium |
| C | Full template extraction (one generic print function, type-dispatch via lambdas) | High effort | Higher — touching 27 functions |

Option A fixes the defect; Option B prevents it from reoccurring. Option C yields diminishing returns given that each function also has unique domain logic.

---

## F3 — `String` Class in FSexplorer.ino Hot Paths (🟠 Debt)

**File:** `FSexplorer.ino`, 17 `String` usages

The `String` class causes heap fragmentation on ESP8266. The project already acknowledges this (see coding conventions). Most dangerous usages are in paths that can be triggered repeatedly:

```cpp
// FSexplorer.ino:291 — called on every file listing request
String dirpath = "/" + String(state.pic.sDeviceid);

// FSexplorer.ino:310-311 — inside a loop over directory entries
String hexfile = dirpath + "/" + dir.fileName();
String verfile = hexfile;

// FSexplorer.ino:447, 459-460 — called during filesystem info display
String sizeStr = formatBytes(dirMap[f].Size);
String usedStr = formatBytes(LittleFSinfo.usedBytes * 1.05);
String totalStr = formatBytes(LittleFSinfo.totalBytes);
```

The `formatBytes()` function at line 540 returns `const String` by value — called in a loop. Each call allocates and immediately frees a heap string.

`doRedirect()` at line 590 takes `String msg` and `String safeURL` by value — unnecessary copies on redirect.

**Note:** `FSexplorer.ino:122` already has a comment "Use a fixed-size line buffer instead of String to avoid heap fragmentation" — so this was partially remediated but incompletely.

**Recommendation:** Replace `formatBytes()` return type with a fixed `char[16]` output buffer (pass-by-pointer pattern). Replace path building `String` concatenations with `snprintf_P`. The `dirpath`, `hexfile`, `verfile` strings need at most 64 bytes.

---

## F4 — `processOT()` is 327 Lines (🟡 Maintainability)

**File:** `OTGW-Core.ino:3334–3660`

`processOT()` handles all incoming serial lines. It was already partially refactored — `decodeAndPublishOTValue()` (line 3299) extracted the OT message dispatch, which is good. What remains is:

1. **Connection liveness tracking** (epochs/state machine for boiler, thermostat, gateway) — ~50 lines
2. **Message routing** (is it OT? PS=1 mode? command echo? error?) — ~80 lines  
3. **Event fan-out** (MQTT + WebSocket publishing of state changes) — ~40 lines
4. **OT frame decode** (call to `decodeAndPublishOTValue`) — already extracted

The connection state machine (items 1+3) repeats the same pattern three times for boiler, thermostat, gateway:
```cpp
// Repeated for boiler, thermostat, and gateway:
state.otgw.bBoilerState = (now < (epochBoilerlastseen + 30));
if ((state.otgw.bBoilerState != bOTGWboilerpreviousstate) || (cntOTmessagesprocessed == 1)) {
  sendMQTTData(F("otgw-pic/boiler_connected"), CCONOFF(state.otgw.bBoilerState));
  bOTGWboilerpreviousstate = state.otgw.bBoilerState;
}
```

**Recommendation:** Extract the connection state tracking into a small helper:
```cpp
static void updateConnectionState(bool& currentState, bool& prevState, 
                                   time_t lastSeen, const __FlashStringHelper* mqttTopic,
                                   bool firstMsg, time_t now);
```
This reduces `processOT()` by ~50 lines and makes the 30-second timeout value a named constant.

---

## F5 — Per-Module Debug Macro Duplication (🟡 Style)

**Files:** `OTGW-Core.ino`, `MQTTstuff.ino`, `restAPI.ino`

Each module defines its own set of conditional debug macros:
```cpp
// In OTGW-Core.ino:
#define OTGWDebugTln(...) ({ if (state.debug.bOTmsg) DebugTln(__VA_ARGS__); })

// In MQTTstuff.ino (identical pattern):
#define MQTTDebugTln(...) ({ if (state.debug.bMQTT) DebugTln(__VA_ARGS__); })

// In restAPI.ino:
#define RESTDebugTln(...) ({ if (state.debug.bRestAPI) DebugTln(__VA_ARGS__); })
```

These are structurally identical, differing only in the flag name. This is a style issue, not a correctness issue — the macros work. The only risk is that a new module forgets to follow this pattern.

**Recommendation:** A single macro generator in `Debug.h` could unify these:
```cpp
#define DEFINE_MODULE_DEBUG(PREFIX, FLAG) \
  ...
```
But this is cosmetic. **Low priority; address only if adding new modules.**

---

## F6 — Dead Code in `processOT()` (🟢 Cleanup)

**File:** `OTGW-Core.ino:3356–3358`

```cpp
static int32_t cntOTmessagesprocessed = 0;
cntOTmessagesprocessed++;
// char _msg[15] {0};
// sendMQTTData(F("otmsg_count"), itoa(cntOTmessagesprocessed, _msg, 10));
```

The message counter is incremented but never published (the publish was commented out). The variable is used only for a `== 1` first-message guard below. The commented-out code suggests this was intentional but never cleaned up.

**Recommendation:** Either re-enable and publish the counter (giving users an OT activity metric), or remove the comment and keep only the variable. One-line fix.

---

## F7 — `String` Class in Upgrade/Flash Paths (🟢 Low-risk Debt)

**File:** `OTGW-Core.ino:4189–4290`

```cpp
String checkforupdatepic(String filename) { ... }
void refreshpic(String filename, String version) { ... }
String hexpath = "/" + String(state.pic.sDeviceid) + "/" + filename;
```

These paths are **infrequently called** (only during firmware upgrade). The `String` usage is not causing crashes in normal operation. However:
- `hexpath` building (line 4234) does 2 allocations and can be replaced with `snprintf_P`
- Function signatures use `String` by value — unnecessary copies
- The `pendingUpgradePath` global (line 4261) is a `String` — fine for a one-time upgrade flow

**Recommendation:** Clean up when convenient, but this is not causing runtime instability.

---

## What Is Already Done Well

Before listing what to change, it is important to acknowledge what is **already solid**:

- **PROGMEM discipline in the hot path** — `OTGW-Core.ino` correctly uses `PSTR()`, `F()`, `snprintf_P`, `PROGMEM_readAnything` throughout. The critical OT message processing loop does not load strings into RAM.
- **`handleOTGW()` uses static buffers** — `sRead[512]` and `sWrite[128]` are `static char`, avoiding stack overflow.
- **MQTT auto-discovery is chunked** — `MQTTstuff.ino` uses 128-byte chunk streaming to avoid heap exhaustion for large payloads.
- **`decodeAndPublishOTValue()` dispatch** — the six `decodeAndPublish*()` functions with `switch` tables are clean, extensible, and readable. This is a well-executed refactor.
- **`restAPI.ino` route dispatch table** — The `kV2Routes[]` table (ADR-050) is a clean data-driven pattern.
- **Command queue** — well-bounded, no dynamic allocation.
- **Settings streaming** — `wStrF/wBoolF/wIntF` helpers avoid heap String usage in settings serialization.
- **MQTT throttle** — the packed `mqttlastsent[]` array with bit-field encoding is clever and RAM-efficient.

---

## Prioritised Recommendation List

| # | Finding | Action | Effort | Gain | Risk |
|---|---------|--------|--------|------|------|
| 1 | F1: Duplicate `jsonEscape` | Remove `OTGW-Core.ino::jsonEscape()`, call `escapeJsonStringTo()` | 30 min | Fixes silent JSON corruption | Very low |
| 2 | F2-A: Missing `publishToSourceTopic` in `print_flag8` | Add missing call (and audit other print functions) | 1–2 h | Fixes observable MQTT bug | Low |
| 3 | F3: `formatBytes()` heap allocation in loop | Change to `char[16]` out-buffer pattern | 1–2 h | Reduces heap churn on file listing | Low |
| 4 | F6: Dead OT message counter | Remove comment or re-enable publish | 15 min | Cleanliness | Trivial |
| 5 | F4: Extract connection state helper | Extract `updateConnectionState()` | 2–3 h | 50-line reduction in processOT | Medium |
| 6 | F2-B: Shared publish helper | Extract `publishScalarOTValue()` | 3–4 h | Prevents future drift | Medium |
| 7 | F7: String in upgrade paths | Replace with snprintf_P | 1–2 h | Code hygiene | Low |
| 8 | F5: Debug macro unification | `DEFINE_MODULE_DEBUG` macro | 1 h | Cosmetic | Low |

---

## Decision Points for Owner

Please decide which of the following to proceed with. I will implement only the approved items.

- [ ] **D1 — Fix F1 (jsonEscape dedup)** — tiny, fixes a correctness bug in JSON output.
- [ ] **D2 — Fix F2-A (missing publishToSourceTopic audit)** — fixes an observable MQTT omission.
- [ ] **D3 — Fix F3 (formatBytes heap)** — reduces heap churn during file browsing.
- [ ] **D4 — Fix F6 (dead code cleanup)** — trivial housekeeping.
- [ ] **D5 — Fix F4 (processOT extraction)** — improves readability, medium scope.
- [ ] **D6 — Fix F2-B (shared publish helper)** — prevents future drift, medium scope.
- [ ] **D7 — Fix F7 (String in upgrade paths)** — low urgency, code hygiene.
- [ ] **D8 — Skip all** — no changes needed, status quo is acceptable.
