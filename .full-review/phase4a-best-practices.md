# Phase 4A: Framework & Language Best Practices

Review baseline: `ace21a48..6a01ae5d` on `feature-dev-2.0.0-otgw32-esp32-sat-support`,
post-fix HEAD `6a01ae5d`. Phase 2 + Phase 3 doc/ADR Highs already landed
(TASK-493 / TASK-494 / TASK-495). Phase 3 H test gap (TASK-496) is in flight
separately.

## Summary

The session work is, on the whole, well-aligned with the project's
embedded-Arduino idioms. PROGMEM hygiene is consistent (the new BLE
discovery helpers use `F()` / `PSTR()` / `snprintf_P` throughout, JSON
is built manually per the no-ArduinoJson rule, and ADR-004 string
discipline holds: `std::string` from NimBLE in scope-bounded blocks
plus `char[]` everywhere else, no Arduino `String` in hot paths).
NimBLE 2.x usage is current — `NimBLEScanCallbacks` (the post-2.0
replacement for the deprecated `NimBLEAdvertisedDeviceCallbacks` /
`BLEAdvertisedDeviceCallbacks`), the 3-arg `start(0, false, true)`
async-continuous pattern, and the `onResult(const NimBLEAdvertisedDevice*)`
override signature all match the NimBLE-Arduino 2.x reference docs.
The TASK-497 `portMUX_TYPE` cross-task guard is correct FreeRTOS usage
for the radio-callback / loop-task split on ESP32-S3.

The findings below are mostly Lows — modernisation opportunities and
small idiom polish. There is one Medium worth surfacing (the C-style
cast chain in the f8.8 conversion deserves either a named helper or a
`static_cast` rewrite for readability), and one Low about the SemVer
range on NimBLE that has a cleaner alternative. No Critical or High
items: the "framework / language idiom" surface area in this diff is
small and the team has been fluent in it.

## Critical findings

(none)

## High findings

(none)

## Medium findings

### 4A-M1 — C-style cast chain in f8.8 conversion is repeated and unsafe-looking

**Files / lines**:
- `src/OTGW-firmware/OTDirect.ino:1956`  (`setRemoteOverride`, post-clamp)
- `src/OTGW-firmware/OTDirect.ino:1851`  (`loopOTDirect`, heating-curve)
- `src/OTGW-firmware/OTDirect.ino:2418`  (`BS=` handler)
- `src/OTGW-firmware/SATble.ino:99,142,151`  (parser raw-to-float widening)

**Pattern**: The float-to-f8.8 conversion is stamped out as
```cpp
uint16_t f88 = (uint16_t)((int16_t)(celsius * 256.0f));
```
in three call sites in `OTDirect.ino` (post-TASK-495 only the one in
`setRemoteOverride()` is range-clamped; the BS= and heating-curve sites
are not). A C-style cast through `(int16_t)` is the most concise way
to get the well-defined two's-complement narrowing here, but:

1. It hides the contract: every call site has to remember that the
   inner `(int16_t)` is what makes the narrowing well-defined; drop it
   and you reintroduce the same UB that 2A-H1 fixed.
2. It is duplicated. KISS allows three similar lines, but four sites
   plus a sibling on the BLE parser side (`int16_t rawTemp = (int16_t)(data[6] | (data[7] << 8));`)
   start to look like a missing inline helper.

**Recommended**: a single `static inline uint16_t floatToF88(float c)`
helper alongside `setOTParityBit()` near the top of OTDirect.ino, with
the clamp and the cast chain in one place. Call sites become
`uint16_t f88 = floatToF88(celsius);`. The clamp would then apply
uniformly to BS= and heating-curve too — closing the narrow-cast UB
risk that is currently latent in those two sites (TASK-495 only fixed
the TT/TC entrypoint).

**Severity rationale**: not a bug today (typical inputs stay in range)
but the contract is fragile and the duplication is real. KISS-aligned
fix is short. This is the closest the diff comes to a real
"language-idiom" improvement. The same observation with severity
"narrow miss UB" appeared as 2A-H1 in Phase 2A; treating it as a
single-line idiom-fix instead of three independent clamps is the
cleaner long-term direction.

### 4A-M2 — `class SATBLEScanCallbacks` could be a singleton-by-value rather than a class with a single `static` instance

**File / line**: `src/OTGW-firmware/SATble.ino:204-288`

**Pattern**: A 90-line class is defined inline only to instantiate one
file-static `_bleScanCallbacks` object. The class has no member state,
no constructor, no other methods. The override is a pure function on
its arguments plus file-statics.

This is fine and idiomatic for NimBLE's interface contract (the
library wants a `NimBLEScanCallbacks*`, and this is the standard
shape). But for code-readers, defining a class to host one method is
heavier than the alternative shapes:

- `struct SATBLEScanCallbacks final : public NimBLEScanCallbacks { ... };` — at least signals "no inheritance, no state" intent.
- The body of `onResult` could call out to a free static
  `parseAndStoreBLEAdvert(...)` to keep the class shell to a half-dozen
  lines and make the parse-and-store logic testable from a host
  fixture (links to TASK-496).

**Recommended**: mark the class `final`. Optionally extract the body
to a free static. Both are zero-runtime-cost and improve readability /
testability.

**Severity rationale**: pure idiom polish; not a bug. Promoted from
Low to Medium only because it is the structural seam that TASK-496's
host-fixture test needs to drive — a free static would be testable
without any NimBLE harness mock.

## Low findings

### 4A-L1 — `enum class` vs unscoped `enum : uint8_t` is inconsistent across new code

**Files**:
- `src/OTGW-firmware/OTDirecttypes.h:40,46,60`  — `enum X : uint8_t` (unscoped)
- `src/OTGW-firmware/MQTTstuff.h:112,125,146,154,207`  — `enum class` (scoped)

**Observation**: The new `OTRemoteOverrideMode` is unscoped, matching
the pre-existing `OTDirectMode` and `OTDirectRequestOrigin` for local
consistency. The Hungarian-prefixed constant names
(`OT_OVERRIDE_TEMPORARY`, `OT_OVERRIDE_CONSTANT`, `OT_OVERRIDE_NONE`)
make collisions unlikely. So this is consistent locally, but the
codebase as a whole has drifted to two styles: HA-discovery in
`MQTTstuff.h` uses `enum class`, while OTDirect uses unscoped.

**Recommended**: no immediate change; if a sweep is ever done it
should use `enum class` going forward. Document as a Low for
consistency record.

### 4A-L2 — `static inline` free function vs `constexpr` opportunity

**File / line**: `src/OTGW-firmware/OTDirect.ino:76-81` (`setOTParityBit`)

**Observation**: The parity helper is `static inline void`; on ESP32
with `-std=gnu++20` it could be `constexpr` (the body is pure-bitwise
on a `uint32_t`). On ESP8266 with GCC 4.8.2 / gnu++11, `constexpr`
free functions need a single return statement, so the current shape
would not survive — confirming the project's "ESP8266 is the
constraint baseline" rule blocks the upgrade.

**Recommended**: leave alone. Not actionable while the dual-target
constraint holds.

### 4A-L3 — Magic numbers in NimBLE scan tuning

**File / lines**: `src/OTGW-firmware/SATble.ino:301-302`
```cpp
_pBLEScan->setInterval(160);   // 100 ms BLE scan-interval
_pBLEScan->setWindow(80);      // 50 ms BLE scan-window (50% radio duty)
```

The unit is "0.625 ms ticks per BLE spec", and the inline comments
correctly translate the result. But promoting the conversion to
`static constexpr uint16_t SCAN_INTERVAL_TICKS = 160;` plus a comment
showing the math (`160 * 0.625 = 100 ms`) would be one step closer to
self-documenting. Same for `setMaxResults(0)` — the magic-zero idiom
"callback-only mode, library never builds a result list" deserves a
named constant.

**Severity rationale**: cosmetic, not a bug. Comments already explain
intent.

### 4A-L4 — `^2.1.0` SemVer range is permissive for an embedded driver

**File / line**: `platformio.ini:165` (`h2zero/NimBLE-Arduino @ ^2.1.0`)

**Observation**: `^2.1.0` resolves to `>=2.1.0, <3.0.0` per
SemVer. NimBLE-Arduino 2.x has had a 2.5.x minor bump since
audit time (the comment says "Pinned to ^2.1.0 (latest 2.x line)"
but does not pin to a specific 2.x patch). For an embedded BLE
driver where a minor bump can subtly change advertisement parsing
or memory layout, a tighter pin (`@ ~2.5.0` for "patch-only") or
an exact pin (`@ 2.5.0`) is more in keeping with the project's
conservatism on other libs (e.g. `WiFiManager @ 2.0.17`,
`PubSubClient @ 2.8.0`, `WebSockets @ 2.3.6` — all exact pins).

**Recommended**: tighten to an exact pin once the current 2.x line
has soaked in beta. ADR-092 already says "a future major bump (3.x)
requires a new ADR" — extending that to "minor bumps within 2.x
require a beta-soak verification" closes the discipline gap.

**Severity rationale**: low impact because the codebase has only one
release-blocking ADR-pinned BLE library. Documented for record.

### 4A-L5 — `(uint8_t)((frame >> 16) & 0xFF)` could be a named accessor

**File / line**: `src/OTGW-firmware/OTDirect.ino:305,310`

**Observation**: The OT-frame bit-field accessors are spelled inline
in `otCmdEnqueue`'s coalesce loop. A `static inline uint8_t
otFrameMsgId(uint32_t frame)` would let the loop read as
`if (otFrameMsgId(otCmdQueue[i]) == otFrameMsgId(frame)) { ... }`.
There is one more identical pattern at line 310 of the same function.

**Recommended**: extract `otFrameMsgId` (and `otFrameType`,
`otFrameData`) — would also help the (planned) host-fixture tests
read OT frames by name rather than by bit-shift.

**Severity rationale**: pure readability; current code is correct and
short.

### 4A-L6 — `portENTER_CRITICAL` / `portEXIT_CRITICAL` symmetry could be RAII-protected

**File / lines**: `src/OTGW-firmware/SATble.ino:262-280, 360-362, 367-370, 463-465, 479-481`

**Observation**: There are five distinct critical sections in
SATble.ino. Every one is balanced today, but a future maintenance edit
could miss a `portEXIT_CRITICAL` on an early-return — and the BLE host
task would be permanently locked. RAII would prevent that:

```cpp
class BLEMuxLock {
public:
  BLEMuxLock(portMUX_TYPE* m) : _m(m) { portENTER_CRITICAL(_m); }
  ~BLEMuxLock() { portEXIT_CRITICAL(_m); }
  BLEMuxLock(const BLEMuxLock&) = delete;
  BLEMuxLock& operator=(const BLEMuxLock&) = delete;
private:
  portMUX_TYPE* _m;
};
```

Patterns like this exist already in the repo (see
`MQTTAutoConfigSessionLock` in `MQTTstuff.ino:75-89`).

**Recommended**: extract a `BLEMuxLock` RAII helper next time SATble.ino
is touched substantially. Not urgent — current code is short and
straight-line.

**Severity rationale**: defensive / future-proofing. KISS allows
the explicit pair today; the suggestion is contingent on more
critical-section call sites appearing.

### 4A-L7 — `(int8_t)dev->getRSSI()` — narrowing cast on already-int8 return

**File / line**: `src/OTGW-firmware/SATble.ino:277, 283`

**Observation**: NimBLE's `getRSSI()` returns `int` for legacy
compatibility, but the value is bounded ±127 dBm by BLE spec.
The `(int8_t)` cast is correct but stylistically `static_cast<int8_t>`
would be more in line with the C++ side of the codebase (which uses
`static_cast` and `reinterpret_cast` consistently elsewhere — see
`MQTTstuff.ino:1959-1960`).

**Recommended**: prefer `static_cast<int8_t>(dev->getRSSI())` in new
code on ESP32. ESP8266 (gnu++11 baseline) does not block this.

**Severity rationale**: cosmetic.

### 4A-L8 — `bleMacToCompact` fixed buffer-size constant is implicit

**File / line**: `src/OTGW-firmware/MQTTstuff.ino:1942-1963`,
caller at `src/OTGW-firmware/SATble.ino:468`

**Observation**: The function takes `(out, outSize)` and validates
`outSize >= 13` internally — good. Caller declares `char macCompact[13];`
— matches. But "13" is a magic constant in two places; if it ever
needs to change (unlikely), only one will be updated.

**Recommended**: define `static constexpr size_t BLE_MAC_COMPACT_SIZE = 13;`
in `OTGW-firmware.h` next to the function declaration; use it in both
the callee's bounds check and the caller's array size.

**Severity rationale**: very low; the value is BLE-spec-fixed.

### 4A-L9 — `else if` chain for TT/TC in `handleOTDirectCommand`

**File / line**: `src/OTGW-firmware/OTDirect.ino:2380-2403`

**Observation**: The TT= / TC= command-dispatch is `else if (cmd0 == 'T'
&& cmd1 == 'T')` and similar — fine for two cases. As the OTDirect
command set grows, the project has a dispatch-table pattern under
ADR-078 (MQTT sub-command dispatch tables). The OTDirect command path
already has 30+ commands and would benefit from the same treatment.
But the diff is small enough that this is YAGNI right now; flagging
for record.

**Recommended**: when a third command in the same category lands, do
the dispatch-table refactor at that point.

**Severity rationale**: pre-existing pattern, not introduced by this
diff. Documented because the diff added two more entries to it.

### 4A-L10 — `do { ... } while (0)` macros are correct but could be inline functions on ESP32

**File / lines**: `src/OTGW-firmware/SATble.ino:30-33`, `OTDirect.ino:28-30`

**Observation**: The `SATBLEDebugTln` / `OTDDebugTln` macros use the
classic `do { ... } while(0)` idiom. With variadic templates on
ESP32 (`-std=gnu++20`), one could write a `constexpr` inline. But the
ESP8266 baseline (gnu++11 + GCC 4.8.2) does not give you the `if
constexpr` pruning you would need to keep the dead-branch out of the
binary, so the macro is the right portable shape today.

**Recommended**: leave alone. ESP8266 baseline blocks the upgrade.

## Strengths observed

- **PROGMEM discipline**: every new string in the BLE discovery
  helpers (`bleSensorPublishOneDiscovery`,
  `bleSensorPublishHaDiscovery`, the four kind-key blocks) uses `F()`
  for `__FlashStringHelper*` parameters and `PSTR()` for `snprintf_P`
  format strings. The reinterpret-cast to `PGM_P` for the `%S` (capital
  S) format specifier is correct and matches Arduino-ESP32 v3.3.5 /
  ESP8266 Core 2.7.4 behaviour.
- **Manual JSON via `snprintf_P`**: 768-byte single-buffer compose,
  bounded, no ArduinoJson, no `String`. Matches the project rule and
  ADR-077 (with the bounded-payload exception correctly called out in
  the function-body comment).
- **ADR-004 in spirit**: NimBLE returns `std::string` by value for
  service-data; the code captures it inside a `{ ... }` scope so
  destruction is bounded. No Arduino-`String` heap churn in the
  hot-path scan callback.
- **NimBLE 2.x current API**: `NimBLEScanCallbacks` (not the deprecated
  `NimBLEAdvertisedDeviceCallbacks`), `setScanCallbacks(cb, true)` for
  per-advert duplicates, `start(0, false, true)` for async-continuous
  scan — all match the NimBLE-Arduino 2.x README and migration guide.
- **FreeRTOS idiom**: `portMUX_TYPE _bleSensorsMux = portMUX_INITIALIZER_UNLOCKED;`
  with `portENTER_CRITICAL` / `portEXIT_CRITICAL` is the canonical
  ESP32 IDF pattern for cross-task short critical sections (correct
  vs. `portENTER_CRITICAL_ISR` which is the ISR-context variant). The
  snapshot-and-process discipline (`BLESensorData snap; ENTER; snap = _bleSensors[i]; EXIT;`)
  keeps the critical section to a single struct-copy — correct.
- **PlatformIO hygiene**: lib_deps are versioned, `lib_ignore` is used
  to exclude libs that should not be auto-compiled per LDF, the
  ctags `-D` workarounds for ESP8266 are documented inline (and the
  rationale survives in `platformio.ini:80-98`). No platformio.ini
  reformat damage at HEAD `6a01ae5d` — earlier 1B-H1 concern proved
  to be a phase-1 false alarm or has since been reverted (verified
  by inspecting the diff vs. ace21a48).
- **Constants over magic**: `OT_OVERRIDE_HONOR_DELTA_F88`,
  `OT_OVERRIDE_RELEASE_DELTA_F88`, `OT_OVERRIDE_HONOR_THRESHOLD`,
  `BTHOME_OBJ_TEMPERATURE_S16`, etc. — every threshold and protocol
  constant has a named `static constexpr` declaration with the math
  shown in the comment.
- **`enum : uint8_t` underlying-type pinning**: All new enums
  explicitly declare `: uint8_t`. This avoids the historical
  issue where `enum X { ... }` defaults to `int` width on ESP8266
  Core 2.7.4 (8-byte-versus-1-byte cost in struct packing).
- **`feedWatchDog()` discipline post-TASK-496**: every return path in
  `bleSensorPublishOneDiscovery` that follows a network attempt now
  feeds the watchdog. Trivial fix shape, correctly applied to the
  4-publish loop and its early-return paths.

## Verdict

The session's framework / language idiom adherence is solid. No
Critical or High findings. Two Medium findings are improvement
opportunities rather than defects (extract f8.8 helper; flag
`SATBLEScanCallbacks` as `final` and consider a free-static parse
function for testability). The Lows are mostly modernisation hints
that the dual-target constraint baseline (ESP8266 GCC 4.8.2 / gnu++11)
correctly blocks.

The team is fluent in the dialect. The diff is well-tuned for the
embedded-Arduino + dual-platform constraint.
