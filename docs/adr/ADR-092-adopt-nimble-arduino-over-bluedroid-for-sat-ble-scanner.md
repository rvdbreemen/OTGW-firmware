# ADR-092 — Adopt NimBLE-Arduino over Bluedroid for SAT BLE Scanner

- **Status**: Accepted
- **Date**: 2026-04-30
- **Tags**: ble, esp32, dependency, sat
- **Supersedes**: (none)
- **Superseded by**: (none)

## Context

TASK-20 added a BLE temperature-sensor scanner to the SAT subsystem
(`src/OTGW-firmware/SATble.ino`), built on the classic Arduino-ESP32 BLE
library exposed via `<BLEDevice.h>`. Internally that library wraps the
Bluedroid Bluetooth host stack shipped with ESP-IDF.

Three problems surfaced in the 2.0.0-alpha cycle:

1. **Flash budget**: Bluedroid pulls in roughly 700 KB of code. On the
   OTGW32 (ESP32-S3, 4 MB flash, 1.5 MB per OTA slot) this competes hard
   with SAT, MQTT, web-assets, OT-core, OTDirect, Ethernet and OLED.
   Estimated savings of switching to NimBLE: ~400 KB of flash, freeing
   roughly a quarter of one OTA slot.

2. **Blocking scan in the main loop**: `BLEScan::start(uint32_t duration,
   bool is_continue)` is the synchronous overload of the classic API. The
   original code passed `false` for `is_continue` and a 3-second
   `duration`, intending non-blocking behaviour but in fact blocking
   `loop()` for 3 seconds every `iBleInterval` seconds. That risks
   software-watchdog margin, MQTT keepalive timeouts, and CMSG-buffer
   overflow on busy OT traffic.

3. **Reference implementation drift**:
   `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp` (the
   reference port we audit against) already uses NimBLE-Arduino 2.x.
   Aligning shrinks our future-merge surface.

## Decision

Adopt **`h2zero/NimBLE-Arduino @ ^2.1.0`** as the BLE host stack on
ESP32 builds. Replace the classic `<BLEDevice.h>` family with
`<NimBLEDevice.h>` in `SATble.ino`. The classic Bluedroid stack is no
longer linked into ESP32 builds.

Specific implementation decisions:

- Use the 3-arg async `start(duration, isContinue, restart)` form so
  `loop()` is never blocked by BLE scanning.
- Use `setMaxResults(0)` so the library never builds a result list; we
  only consume callbacks. Saves heap.
- Use scan interval/window 160/80 (50% radio duty during the 3-second
  scan window), aligned with OT-Thing.
- Periodic-scan model retained: a scan starts every
  `settings.sat.iBleInterval` seconds with a duration of
  `BLE_SCAN_DURATION_SEC = 3` seconds. The user-tunable knob is
  preserved.
- Keep both ATC/pvvx (UUID `0x181A`) and BTHome v2 (UUID `0xFCD2`)
  parsers. Byte-layout of both formats is unchanged.
- BTHome v2: now also validates the version-bit (`0x40`) and rejects
  encrypted advertisements (`0x01`); this was implicit before.
- Service-data extraction switches from Arduino `String` to
  `std::string`, eliminating heap-churn on every advertisement.
  Removes a soft ADR-004 violation.
- Pin to `^2.1.0` so future patch releases auto-apply but a major-version
  bump (3.x) requires a new ADR.

## Alternatives considered

### Stay on Bluedroid, fix only the blocking call

Cheap; replace the 2-arg `start(duration, false)` with the 3-arg async
form `start(duration, scanCompleteCB, false)`. Does not address the
flash-budget pressure. Defers a forced rework to the next time the
4 MB partition fills up. Rejected.

### Migrate to NimBLE 1.4.x (legacy)

NimBLE 1.4.x is in maintenance mode upstream and uses the deprecated
`BLEAdvertisedDeviceCallbacks` API. NimBLE 2.x is the active line and
matches what OT-Thing already uses. Rejected.

### Provide both stacks and let the user pick

Doubles maintenance burden, doubles the binary footprint of test builds,
and doubles the surface for stack-specific bugs. The two stacks cannot
co-exist in the same firmware binary anyway: ESP-IDF refuses to link
both Bluedroid and NimBLE simultaneously. Rejected.

## Consequences

**Positive**:

- Approximately 400 KB of flash freed in ESP32 builds. Concrete delta
  recorded in the TASK-487 PR description.
- BLE scan no longer blocks `loop()`; software-watchdog margin restored,
  no risk of MQTT keepalive timeouts during BLE scan windows.
- Hot-path Arduino-`String` allocations in the scan callback are
  eliminated. Aligned with ADR-004.
- Reference parity with OT-Thing-OTGW32 reduces future-merge friction.

**Negative / risks**:

- One-way migration: rolling back requires this ADR to be superseded
  and the SATble.ino code reverted. There is no runtime fallback.
- NimBLE 2.x has API differences from the classic stack: the scan
  callback now takes `const NimBLEAdvertisedDevice*` (pointer + const)
  instead of `BLEAdvertisedDevice` by value. Compile-time enforced;
  detectable at build time.
- BLE on Bluedroid was the stack the early TASK-20 testers ran on.
  Hardware verification on real OTGW32 with both ATC/pvvx and BTHome v2
  sensors is mandatory before this change ships in a release
  (TASK-487 AC #6).

## Verification gates

- **Completeness**: Context, Decision, Alternatives, and Consequences
  are explicit. Pinned version specified. Scope contained: SATble.ino
  + `[env:esp32]` lib_deps in platformio.ini.
- **Evidence**: Bluedroid blocking-call behaviour documented at the
  pre-change `SATble.ino` line where `_pBLEScan->start(BLE_SCAN_DURATION_SEC, false)`
  is called; the Arduino-ESP32 BLEScan signature confirms the 2-arg
  form is synchronous (`BLEScanResults start(uint32_t, bool)`).
  OT-Thing reference at
  `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp:355-365`
  shows the NimBLE 2.x API in use (`pBLEScan->start(0, false, true)`).
- **Clarity**: No ambiguous "may", "consider", or "should ideally"
  verbs in the Decision section. Pinned version is explicit
  (`^2.1.0`).
- **Consistency**: ADR-004 (no `String` in hot paths) is reinforced,
  not violated. ADR-051 (settings/state architecture) is not affected.
  ESP8266 build is unchanged: BLE code is already inside
  `#if defined(ESP32)`.

## Related

- TASK-20 — original BLE temperature-sensor implementation
- TASK-487 — implementation of this decision
- TASK-488 — follow-up: HA auto-discovery for BLE sensors (depends on
  this ADR landing first)
- ADR-004 — no `String` in hot paths
- ADR-080 — binding ADR rules and CI gates (this ADR is structural;
  reviewed at PR, no automated gate is feasible since the change is a
  one-line lib_deps addition plus a file rewrite)
- `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp` — reference
  implementation
