# ESP Abstraction Leak Audit — feature-dev-2.0.0-otgw32-esp32-sat-support

**Date:** 2026-05-28
**Auditor:** Claude (senior C++/ESP IoT review pass, on behalf of @rvdbreemen)
**Branch audited:** `feature-dev-2.0.0-otgw32-esp32-sat-support` @ `9be88a0d`
**Backlog:** TASK-739 (this deliverable) → TASK-740…746 (Tier 0–6 remediation)

## 1. Purpose

The 2.0.0 branch carries an explicit ESP8266/ESP32 abstraction layer so that
application code can be written against `platformXxx()` shims and `HAS_*`
capability flags, never against raw `ESP8266` / `ESP32` /
`ARDUINO_ARCH_ESP*` / `BOARD_NODOSHOP_ESP*` preprocessor symbols. The
contract is that *no* such conditional exists outside the abstraction-layer
files. This audit measures the gap.

## 2. Abstraction layer files (allowed to contain platform `#ifdef`)

| File | Role |
|---|---|
| `src/OTGW-firmware/platform.h` | dispatcher → selects platform header |
| `src/OTGW-firmware/platform_esp8266.h` | ESP8266 includes, shims, type aliases |
| `src/OTGW-firmware/platform_esp32.h` | ESP32 includes, shims, type aliases |
| `src/OTGW-firmware/boards.h` | pin maps + `HAS_*` capability flags |
| `src/OTGW-firmware/OTGW-ModUpdateServer{.h,-esp32.h,-impl.h}` | parallel mini-abstraction (dispatcher + per-platform impl) for the HTTP update server |

Anything else is application code and must not branch on the platform.

## 3. Baseline

**78 violations across 18 files** as of 2026-05-28 / commit `9be88a0d`.

| Count | File |
|---:|---|
| 12 | `src/OTGW-firmware/SATweather.ino` |
| 8 | `src/OTGW-firmware/SATcycles.ino` |
| 8 | `src/OTGW-firmware/jsonStuff.ino` |
| 8 | `src/OTGW-firmware/restAPI.ino` |
| 6 | `src/OTGW-firmware/MQTTstuff.ino` |
| 6 | `src/OTGW-firmware/handleDebug.ino` |
| 5 | `src/OTGW-firmware/OLED.ino` |
| 4 | `src/OTGW-firmware/SATcontrol.ino` |
| 4 | `src/OTGW-firmware/networkStuff.ino` |
| 3 | `src/OTGW-firmware/OTGW-firmware.h` |
| 3 | `src/OTGW-firmware/SATtypes.h` |
| 2 | `src/OTGW-firmware/OTGW-firmware.ino` |
| 2 | `src/OTGW-firmware/SATmqttPublish.h` |
| 2 | `src/OTGW-firmware/settingStuff.ino` |
| 2 | `src/libraries/OTGWSerial/OTGWSerial.cpp` |
| 1 | `src/OTGW-firmware/OTDirect.ino` |
| 1 | `src/OTGW-firmware/SATble.ino` |
| 1 | `src/OTGW-firmware/helperStuff.ino` |

`evaluate.py` enforces this baseline via the `check_esp_abstraction_boundary()` gate.
The check FAILs if the count rises and WARNs while any violation remains.
When the count reaches zero the gate becomes a permanent invariant.

### 3.1 Baseline reconciliation — TASK-756 (2026-05-29)

After the baseline was recorded the live count drifted to **85** (a +7 regression
caught by the gate), from two sources:

- **`src/libraries/SimpleTelnet/src/SimpleTelnet.h` (+4).** SimpleTelnet is an
  independent, self-contained vendored library (own upstream repo, own
  portability layer); it is not OTGW application code. The abstraction-boundary
  rule governs application code, so the scanner now **excludes** it via
  `ESP_ABSTRACTION_EXCLUDED_LIB_DIRS` in `evaluate.py`. The library is not
  modified.
- **`src/OTGW-firmware/settingStuff.ino` (+3).** The TASK-564 settings no-op
  detection added raw `#if defined(ESP8266)/ESP32` guards (ESP8266 CRC32
  sentinel vs ESP32 full-struct snapshot). Remediated by moving the divergence
  behind the `platformSettingsNoopCapture()` / `platformSettingsNoopUnchanged()`
  shims in `platform_esp8266.h` / `platform_esp32.h`; each platform's deliberate
  strategy is preserved. The `#include <coredecls.h>` guard moved into
  `platform_esp8266.h`.

After both, the count is back to **78** (the recorded baseline), so the baseline
number is unchanged and remains accurate — it was restored by remediation, not
raised to mask the regression. The 2 settingStuff entries still counted are the
original baseline BLE-serialization guards (`#if defined(ESP32)`), owned by the
Tier remediation tasks (TASK-740..746), not by this reconciliation.

## 4. Findings grouped by remediation pattern

Grouped this way because one fix often kills many violations.

### A. Whole-module feature files pretending to be platform modules — HIGH

These files exist *because* the feature is ESP32-only. Wrap them with **one
feature flag** at the top, drop every inner `#ifdef`:

| File | Inner ESP-conditionals | Suggested gate (`boards.h`) |
|---|---:|---|
| `SATble.ino` | 1 (file-level, line 24) | `HAS_SAT_BLE` |
| `SATweather.ino` | 11 (lines 35, 63, 176, 192, 223, 228, 275, 396, 579, 612, 687, 701) | `HAS_WEATHER_FORECAST` (independent of SAT) |
| `Ethernet.ino` | already gated by `HAS_ETH_CAPABLE` | model for the others |

`SATweather.ino` is the worst offender: it has 11 *individual* `#ifndef
ESP8266` blocks inside one file. Replace them with one outer `#if
HAS_WEATHER_FORECAST` and let the dead code disappear cleanly.

→ Tackled by **Tier 1** (TASK-741).

### B. Struct-field divergence in shared headers — HIGH (schema hazard)

| File:line | What diverges | Risk |
|---|---|---|
| `SATtypes.h:335-344` | `state.sat.SATRuntimeSection` BLE fields only exist on ESP32 | Code reading these fields must replicate the `#ifdef`, multiplying leaks |
| `SATtypes.h:461-475` | Persistent `OTGWSettings.sat` BLE fields only exist on ESP32 | `settings.json` no longer round-trips between platforms — silent data loss |

Fix: either (a) include the fields unconditionally and zero them on ESP8266
— the 30-byte RAM / 220-byte flash cost is trivial — or (b) hoist
`SATtypes.h` itself under `#if HAS_SAT`. Whichever you pick, every
downstream `#if defined(ESP32)` in category **F** below collapses to
nothing.

→ Tackled by **Tier 2** (TASK-742).

### C. Per-platform tuning constants scattered through `.ino` files — LOW (quick win)

| Constant | Site | Belongs in |
|---|---|---|
| `SAT_WIN4H_SIZE` | `SATtypes.h:100-104` | `platform_*.h` |
| flow sample sizes | `SATcycles.ino:47/62/68/79` | `platform_*.h` |
| `HCR_DAYS` / `HCR_INTRADAY_SIZE` / index types | `SATcycles.ino:139/146/164` | `platform_*.h` |
| `SAT_CYCLES_FILE_BUF_SIZE` | `SATcycles.ino:1115` | `platform_*.h` |
| `MQTT_DISCOVERY_HEAP_MIN` | `MQTTstuff.ino:57-61` | `platform_*.h` |
| `STATUS_BURST_COOLDOWN_MS` | `MQTTstuff.ino:136-140` | `platform_*.h` |

Pattern is always `#if defined(ESP8266) define X 30 #else define X 360
#endif`. Move both arms to the appropriate `platform_*.h`; the application
file just consumes `X`.

→ Tackled by **Tier 3** (TASK-743).

### D. Missing shims for divergent APIs — MEDIUM-HIGH

These need one new `platformXxx()` function per pair. After they exist,
the caller is one line, unguarded.

| Capability | Sites | Proposed shim |
|---|---|---|
| LED PWM/GPIO | `OTGW-firmware.ino:247-307` (60-line block, `BOARD_NODOSHOP_ESP32` guarded) | `platformSetLed(led,status)` / `platformBlinkLed(led)` |
| JSON tx coalescing | `jsonStuff.ino:191-263` (5 sites) | `platformRestTxAppend/Reset/Flush/AppendProgmem()` |
| Heap-health predicate | `MQTTstuff.ino:1898-1924` (also bypasses shim with raw `ESP.getFreeHeap()` at :1922) | `platformHeapHasPressure()` / `platformHeapIsHealthyForRestore()` |
| WiFi encryption enum | `restAPI.ino:1942` | `platformWiFiIsEncrypted(uint8_t)` |
| WiFi-client setSync | `MQTTstuff.ino:588` | `platformWiFiClientConfigureSync(WiFiClient&)` |
| NTP / hostname-reset workaround | `networkStuff.ino:610-624` | `platformStartNTP()` (encapsulates the SDK bug) |
| Reset reason → PIC char | `OTDirect.ino:3076-3086` | `platformGetResetReasonChar()` |
| HardwareSerial(PIC) begin | `OTGWSerial.cpp:835-847` (first-party lib) | `platformPicSerialBegin(serial)` |
| Random source | `SATmqttPublish.h:46/63` | `platformRandom()` |

→ Tackled by **Tier 3** (TASK-743).

### E. Missing stubs for ESP32-only feature functions — MEDIUM

Once category **A** is in place with `HAS_SAT_BLE`, also declare empty
inline stubs for the SAT-BLE API in `platform_esp8266.h` (or in a
`HAS_SAT_BLE`-guarded header). Each call site then drops its `#if
defined(ESP32)`. Affected sites:

- `OTGW-firmware.ino:644` — `satBLELoop()`
- `OTGW-firmware.h:284-313` — forward declarations
- `SATcontrol.ino:975/1948/2559/3042` — `satBLEGetTemp / satBLESendStatusJSON / satBLEPublishMQTT / satBLEInit`
- `settingStuff.ino:384-401` and `:1002-…` — BLE settings serialize/parse (extract to `platformSerializeBleSettings` / `platformParseBleSettings`)
- `networkStuff.ino:514-539` — debug telemetry / menu items
- `handleDebug.ino:26, 184, 236, 257, 298, 401` — heap+BLE+menu+key handler

→ Tackled by **Tier 2** (TASK-742).

### F. Application code reading board macros directly — LOW

| Site | Issue |
|---|---|
| `OTGW-firmware.ino:247` | `#if defined(BOARD_NODOSHOP_ESP32)` for LED block — disappears once `platformSetLed()` exists. |
| `OTGW-firmware.h:543-551` | `boardName()` switches on `BOARD_NODOSHOP_*` — replace with `#define BOARD_NAME_PROGMEM "..."` per board in `boards.h`. |

→ Tackled by **Tier 3** (TASK-743) — falls out of the LED shim work.

### G. OLED FreeRTOS coupling — MEDIUM (latent)

`OLED.ino:42, 81, 91, 425, 452` uses `QueueHandle_t`, `xQueueCreate`,
`xQueueReceive`, `gpio_set_intr_type`, `IRAM_ATTR`, `esp_timer_get_time`
etc. directly. Today this compiles only because `HAS_OLED_CAPABLE` is
ESP32-only — but the *inner* `#if defined(ESP32)` checks are belt-and-
braces that contradict the principle. If an ESP8266 board ever gains an
OLED, all of `OLED.ino` breaks. Fix now via `platformOledInitButton(pin,
isrFn)` / `platformOledDrainButton(int64_t&)` shims; move the ISR + queue
into `platform_esp32.h`.

→ Tackled by **Tier 4** (TASK-744).

### H. Bypassing existing shims with raw `ESP.getXxx()` — LOW per site, quietly corrosive

These sites have an available `platformXxx()` shim and ignore it. No
`#ifdef` is even needed — just a substitution:

| File:line | Wrong | Right |
|---|---|---|
| `restAPI.ino:1639` | `ESP.getFreeHeap()` | `platformFreeHeap()` |
| `restAPI.ino:1641/1642` | `ESP.getMinFreeHeap()` / `getMaxAllocHeap()` | `platformMinFreeHeap()` / `platformMaxFreeBlock()` |
| `restAPI.ino:1644` | `ESP.getHeapFragmentation()` | `platformHeapFragmentation()` |
| `restAPI.ino:1646` | `ESP.getMaxFreeBlockSize()` | `platformMaxFreeBlock()` |
| `handleDebug.ino:25/27/28/30/32` | mixed `ESP.*` heap APIs inside an `#ifdef` block | call shims, drop the `#ifdef` |
| `OLED.ino:279` | `ESP.getFreeHeap()` | `platformFreeHeap()` |
| `MQTTstuff.ino:1922` | `ESP.getFreeHeap()` (inside the ESP8266 arm of a heap predicate) | `platformFreeHeap()` |
| `OTGW-ModUpdateServer-impl.h:195` | `ESP.getFreeSketchSpace()` | acceptable — this file is ESP8266-only by dispatch |
| `Ethernet.ino:42` | `ESP.getEfuseMac()` | acceptable — file is `HAS_ETH_CAPABLE`-gated and ESP32-only; could still go through a `platformGetMacU64()` shim for symmetry |

→ Tackled by **Tier 1** (TASK-741) — quick `sed`-level wins.

### I. Type-ambiguity overloads — LOW (defensible)

`jsonStuff.ino:389, 487, 660` define extra `sendJsonMapEntry(int)` /
`(unsigned int)` overloads on ESP32 because xtensa-esp32 distinguishes
`int` from `int32_t` (a `long` there). Three resolution paths:

1. Cast all call sites to `int32_t/uint32_t` explicitly — eliminates the
   overloads and the `#ifdef` (preferred, ~50 callsite edits).
2. Wrap the overloads in a `JSON_INT_OVERLOADS_NEEDED` macro set per-
   platform inside `platform_*.h` — moves the conditional into the
   abstraction tier without changing call sites.
3. Refactor `sendJsonMapEntry` into a template (most invasive).

→ Tackled by **Tier 5** (TASK-745).

## 5. Prioritised remediation roadmap

| Tier | Task | Action | Eliminates | Effort |
|---|---|---|---:|---|
| **0** | TASK-740 | Add `HAS_SAT_BLE`, `HAS_WEATHER_FORECAST`, `HAS_SAT` flags to `boards.h` | infra for tiers 1-3 | 30 min |
| **1** | TASK-741 | Collapse `SATweather.ino`'s 11 `#ifndef ESP8266` to one `#if HAS_WEATHER_FORECAST`; flip `SATble.ino` file guard to `#if HAS_SAT_BLE`; substitute raw `ESP.getXxx()` → existing shims | ~22 sites | 2 hr |
| **2** | TASK-742 | Unify `SATtypes.h` struct fields; add no-op SAT-BLE stubs in `platform_esp8266.h`; strip all `#if defined(ESP32)` from SAT-BLE call sites; factor BLE settings I/O | ~15 sites | half day |
| **3** | TASK-743 | Move tuning constants into `platform_*.h`; build all new shims (LED, NTP, reset-reason, WiFi enum, JSON tx, heap pressure, PIC serial, etc.) | ~15 sites | 1 day |
| **4** | TASK-744 | OLED platform-shim refactor (queue, ISR, drain) | 5 sites | half day |
| **5** | TASK-745 | Resolve `jsonStuff.ino` overload ambiguity | 3 sites | half day |
| **6** | TASK-746 | Promote `platform.h` / `boards.h` / `platform_*.h` into `src/libraries/Platform/` so the boundary is physical, not nominal; ADR documenting the contract | structural | 1 day |

After Tier 3 the violation count should be **under 5** (residual
`BOARD_NODOSHOP_ESP*` dispatch logic inside `boards.h` itself). After
Tier 6 the count is zero and the gate becomes a permanent invariant.

## 6. Guardrail

`evaluate.py` gains `check_esp_abstraction_boundary()` (TASK-739):

- WARN whenever the violation count is non-zero (visible cost).
- FAIL whenever the count rises above the baseline (regression gate).
- PASS at zero.

Baseline is encoded as `ESP_ABSTRACTION_BASELINE = 78` in `evaluate.py`.
Each tier task must update the constant downward as part of its DoD.

The same regex is documented in `CLAUDE.md` and `AGENTS.md` so new code
written by humans or agents is shaped to clear the gate before commit.

## 7. References

- TASK-739 (this deliverable) and TASK-740…746 (tier remediation)
- `src/OTGW-firmware/platform.h` and the rest of the abstraction layer
- `evaluate.py::check_esp_abstraction_boundary()`
- `CLAUDE.md` § Critical Coding Rules → ESP Platform Abstraction
- `AGENTS.md` § Architecture rules → ESP Platform Abstraction
