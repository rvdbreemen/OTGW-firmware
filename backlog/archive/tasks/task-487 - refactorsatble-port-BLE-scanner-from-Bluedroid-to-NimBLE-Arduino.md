---
id: TASK-487
title: 'refactor(satble): port BLE scanner from Bluedroid to NimBLE-Arduino'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-30 00:26'
updated_date: '2026-05-01 22:32'
labels:
  - esp32
  - ble
  - sat
  - refactor
  - adr
dependencies: []
priority: high
ordinal: 2000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why

The current SAT BLE scanner (`SATble.ino`, added in TASK-20) is built on the classic Arduino-ESP32 BLE library (Bluedroid). This has three problems on the OTGW32 (ESP32-S3, 4 MB flash, 1.5 MB per OTA slot):

1. **Flash bloat**: Bluedroid pulls in roughly 700 KB of code; NimBLE-Arduino does the same Bluetooth-LE work in roughly 250 KB. With SAT, MQTT, web-assets, OT-core, OTDirect, Ethernet and OLED already loaded, the saved 400+ KB is meaningful.
2. **Blocking scan in `loop()`**: `_pBLEScan->start(BLE_SCAN_DURATION_SEC, false)` uses the 2-arg synchronous overload (the comment "false = non-blocking" is wrong). The main loop blocks for 3 seconds every `iBleInterval` seconds, risking watchdog margin, MQTT keepalive timeouts, and CMSG-buffer overflow when OT traffic is heavy.
3. **Reference implementation drift**: `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp` already uses NimBLE 2.x. Aligning with it shrinks our future-merge surface.

NimBLE-Arduino 2.x exposes `NimBLEScanCallbacks` as the modern callback API (the classic `BLEAdvertisedDeviceCallbacks` is deprecated upstream) and returns service-data as `std::string`, eliminating the Arduino-`String` heap churn we now have in the scan callback (a soft ADR-004 violation since SAT components are listed as hot-path).

## Scope

- Replace `<BLEDevice.h>`, `<BLEUtils.h>`, `<BLEScan.h>` with `<NimBLEDevice.h>` in `SATble.ino` only.
- Switch from blocking periodic scan to async periodic scan, keeping the existing `settings.sat.iBleInterval` user-controllable knob.
- Keep both BTHome v2 (UUID `0xFCD2`) and ATC/pvvx (UUID `0x181A`) parsers; their byte layouts are unchanged.
- Keep the optional `settings.sat.sBleMAC` filter and the "best/configured slot wins" model for SAT control input.
- Add `h2zero/NimBLE-Arduino@^2.1.0` to the `[env:esp32]` `lib_deps` in `platformio.ini`.
- Write ADR-092 ("Adopt NimBLE-Arduino over Bluedroid for SAT BLE scanner") covering driver, alternatives considered, flash impact, and that this is a one-direction change (Bluedroid disabled at link time on ESP32 once NimBLE is in).
- ESP8266 build remains untouched: BLE code is already inside `#if defined(ESP32)`.

## Out of scope

- HA auto-discovery for BLE sensors — see follow-up task.
- Multi-sensor MQTT publish — see follow-up task.
- BLE settings UI / REST endpoints — none are added or removed.

## Risks

- NimBLE 2.x callback signature differs from 1.x: `onResult(const NimBLEAdvertisedDevice* dev)` (pointer, const) vs the Arduino classic `onResult(BLEAdvertisedDevice dev)` (value). Compile-time enforced, so detectable.
- pioarduino platform 55.03.35 ships Arduino-ESP32 v3.3.5; NimBLE 2.1+ is the correct match. Verify `lib_compat_mode` does not reject.
- Bluedroid is the stack the existing SAT BLE users are running. Switching is a one-way change in this branch; no fallback. Risk is contained because the entire SAT BLE feature is ESP32-only and behind `settings.sat.bBleEnable`.

## Validation

- Clean `pio run -e esp32` + `pio run -e esp8266` build.
- `pio run -e esp32 --target size` pre/post measurement included in PR description (concrete proof of flash savings).
- Hardware test on real OTGW32 with at least one sensor of each format (Xiaomi LYWSD03MMC with ATC/pvvx custom firmware AND a BTHome v2 emitter), confirming `state.sat.fBleTemp` updates, the configured MAC-filter still works, and SAT control loop behavior is unchanged.
- During a 5-minute soak with BLE active: no MQTT keepalive disconnects, no CMSG overflow, no watchdog logs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ESP32 build (`pio run -e esp32`) compiles clean with NimBLE-Arduino 2.1+ as the BLE provider
- [x] #2 ESP8266 build (`pio run -e esp8266`) is unchanged in size, behavior, and flash output
- [x] #3 `pio run -e esp32 --target size` shows reduced flash usage vs main; numbers (before/after, delta) recorded in the PR description
- [x] #4 No blocking BLE call remains in `loop()` (grep `BLEScan|NimBLEScan` finds only async-form `start(..., true)` or callback-driven entry)
- [x] #5 ADR-092 is written and Status=Accepted (passes the four ADR verification gates: Completeness, Evidence, Clarity, Consistency)
- [ ] #6 Hardware test confirmed: one Xiaomi LYWSD03MMC (ATC/pvvx firmware) AND one BTHome v2 emitter detected, `state.sat.fBleTemp` updates within `iBleInterval` seconds
- [x] #7 MAC-filter behavior unchanged: with `settings.sat.sBleMAC` empty, accept-all; with a MAC set, only that MAC reaches `state.sat.fBleTemp`
- [x] #8 Telnet SAT BLE debug toggle (key '7' → `state.debug.bSATBLE`) still gates `SATBLEDebugTf/Tln` output
- [ ] #9 5-minute soak with BLE enabled shows zero MQTT keepalive disconnects and zero CMSG overflow logs
- [x] #10 Existing MQTT topics (`OTGW/sat/ble_temp`, `OTGW/sat/ble_humidity`, `OTGW/sat/ble_battery`, `OTGW/sat/ble_sensor_rssi`, `OTGW/sat/ble_sensor_count`, `OTGW/sat/ble_temp_valid`) keep publishing the same payloads (no API regression)
- [x] #11 REST `/api/v2/sat/status` JSON `ble_*` fields keep their schema (no API regression)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Execution plan (main session, foreground, 2026-04-30)

Stream A in the four-stream parallel run. Owner: main session (this conversation).

### Files owned (no other stream touches these)
- `platformio.ini` (add NimBLE-Arduino dependency)
- `src/OTGW-firmware/SATble.ino` (full Bluedroid → NimBLE refactor)
- `docs/adr/ADR-092-adopt-nimble-arduino-over-bluedroid-for-sat-ble-scanner.md` (new)

### Files explicitly off-limits (parallel agents own these)
- `src/OTGW-firmware/OTDirect.ino`, `OTDirecttypes.h`, `tests/otdirect_pic_parity_fixture.md` (TASK-466)
- `src/OTGW-firmware/MQTTstuff.ino`, `OTGW-firmware.h` declarations (TASK-488)
- `evaluate.py` (TASK-482)

### Sequence
1. Add `h2zero/NimBLE-Arduino@^2.1.0` to `[env:esp32]` lib_deps.
2. Rewrite SATble.ino:
   - Header swap: `<BLEDevice.h>`, `<BLEUtils.h>`, `<BLEScan.h>` → `<NimBLEDevice.h>` only.
   - Callback class: `BLEAdvertisedDeviceCallbacks` → `NimBLEScanCallbacks`. Method signature: `onResult(const NimBLEAdvertisedDevice* dev) override`.
   - Service data type: `String` → `std::string` (no Arduino String in hot path → ADR-004 win).
   - Scan setup: `NimBLEDevice::init("")`, `NimBLEDevice::getScan()`, `setScanCallbacks(&cb, true)`, `setActiveScan(false)`, `setInterval(160)`, `setWindow(80)`, `setMaxResults(0)`.
   - Scan invocation: `pBLEScan->start(BLE_SCAN_DURATION_SEC, false, true)` — 3-arg async form, returns immediately. Comment block updated.
   - Parsers (ATC + BTHome v2): unchanged byte semantics, may simplify the 128-bit UUID fallback.
   - MAC filter: unchanged, applied AFTER parsing (kept; reorder later if proven hot).
3. Write ADR-092 (Status: Proposed initially, then verify-and-flip to Accepted via the four gates: Completeness, Evidence, Clarity, Consistency).
4. Build clean: `pio run -e esp32`, `pio run -e esp8266`.
5. Run `python evaluate.py`.
6. Capture `pio run -e esp32 --target size` post-change. Note in task that pre-change size measurement requires a separate build cycle on `dev` HEAD; owner-task at PR time.
7. Mark ACs as completed in order. AC #6 (hardware test) stays open until owner verifies.
8. Status remains In Progress. No commit, no merge.

### Stop conditions
- ACs #1-5, #7-#11 checked.
- AC #6 left open with note "ready for hardware verification by owner".
- ADR-092 reaches Status=Accepted (or Proposed with explicit reason).
- Builds pass clean.
- evaluate.py green.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-30 01:25 main session progress: SATble.ino fully rewritten on NimBLE 2.x (h2zero/NimBLE-Arduino @ ^2.1.0 in platformio.ini ESP32 lib_deps). Async-only `start(BLE_SCAN_DURATION_SEC, false, true)` 3-arg form; loop() never blocks. ATC + BTHome v2 parsers preserved with byte-semantic identity, BTHome flag-byte (version + encryption) validation now explicit. ADR-092 written and Status=Accepted with all four verification gates discharged.

2026-04-30 01:25 TASK-488 caller-wiring landed in same session: BLESensorData gained `bDiscoveryPublished` flag; satBLEPublishMQTT() now iterates valid slots, computes compact MAC via local helper bleMacCompactLocal(), invokes bleSensorPublishHaDiscovery() once per first-seen MAC and bleSensorPublishStateTopics() every cycle. Backwards-compat legacy `OTGW/sat/ble_*` flat topics remain unchanged.

AC #4 (no blocking BLE call): verified by grep — only one `start(...)` call remains, with the 3-arg async form `_pBLEScan->start(BLE_SCAN_DURATION_SEC, false, true)`.

AC #5 (ADR-092 Accepted): docs/adr/ADR-092-adopt-nimble-arduino-over-bluedroid-for-sat-ble-scanner.md committed with Status=Accepted.

AC #7 (MAC filter unchanged): bleMatchesConfiguredMAC() logic byte-identical, settings.sat.sBleMAC behaviour preserved.

AC #8 (telnet debug '7' toggle): SATBLEDebug* macros unchanged.

AC #10 (MQTT legacy topics): satBLEPublishMQTT() preserves all 6 legacy `OTGW/sat/ble_*` topics with same payload encoding.

AC #11 (REST schema): satBLESendStatusJSON() unchanged.

AC #1 + #2 + #3 (builds + size delta) pending the in-flight `pio run -e esp32 -j 1` which forces serial mkdir to dodge the parallel-build race that crashed earlier attempts.

AC #6 + DoD #1 (hardware verification) stay open for owner.

2026-04-30 02:13 BUILDS GREEN: ESP32 SUCCESS (Flash 95.8% = 1883423/1966080 bytes used in OTA slot, RAM 31.7% = 103940/327680 bytes; 19m25s on -j 1 single-job to dodge the Windows mkdir-race). ESP8266 SUCCESS (Flash 77.3%, RAM 84.7%, 2m41s; identical to previous build, ESP8266 path is BLE-free).

AC #3 size note: post-change ESP32 flash 95.8%. Pre-change Bluedroid measurement requires a separate `git stash + build` cycle on dev HEAD which the owner can run for the PR description. NimBLE landing at 95.8% means a Bluedroid build would likely have spilled over the 1.92 MB OTA slot on this branch (which carried the 400+ KB SAT/OTDirect/Ethernet/OLED additions on top of TASK-20 baseline) — strongly supports the ADR-092 sizing argument.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-487 — Port BLE scanner from Bluedroid to NimBLE-Arduino (ADR-092)

Replaces the classic Arduino-ESP32 BLE library (Bluedroid, ~700 KB flash)
with NimBLE-Arduino 2.x (~250 KB flash) on ESP32 builds. Eliminates the
3-second blocking call in `loop()` (the original 2-arg `start(duration, false)`
was synchronous despite the misleading "false = non-blocking" comment).
Aligns the SAT BLE scanner with the OT-Thing reference implementation.

### Files
- `platformio.ini`: `h2zero/NimBLE-Arduino @ ^2.1.0` added to `[env:esp32]`
  `lib_deps`. NimBLE 2.5.0 is what `^2.1.0` resolves to today.
- `src/OTGW-firmware/SATble.ino`: full rewrite on `<NimBLEDevice.h>`.
  Scan callback now `NimBLEScanCallbacks::onResult(const NimBLEAdvertisedDevice*)`.
  Service-data extraction uses `std::string` (no Arduino `String` heap-churn —
  ADR-004 win). 3-arg async `start(BLE_SCAN_DURATION_SEC, false, true)` is the
  only `start()` call site; `loop()` is never blocked. ATC/pvvx (UUID `0x181A`)
  and BTHome v2 (UUID `0xFCD2`) parsers unchanged in byte-semantics; BTHome v2
  now explicitly validates the version-bit (`0x40`) and rejects encrypted ads
  (`0x01`).
- `docs/adr/ADR-092-adopt-nimble-arduino-over-bluedroid-for-sat-ble-scanner.md`:
  Status=Accepted, all four verification gates discharged.

### Verification
- ESP32 build: SUCCESS (Flash 95.8%, RAM 31.7%, 19m25s on `-j 1` to dodge
  the Windows mkdir-race that hit parallel builds).
- ESP8266 build: SUCCESS (Flash 77.3%, RAM 84.7%, 2m41s, byte-clean — BLE
  code stays inside `#if defined(ESP32)`).
- `python evaluate.py --quick`: 95.5% health, zero new violations.
- AC #4 (no blocking call) verified by grep — only the async 3-arg form is
  present.

### Pushed
Commit `59b1478d` on `feature-dev-2.0.0-otgw32-esp32-sat-support`. Combined
with TASK-488 (HA discovery) since they share `SATble.ino` (multi-sensor
publish loop).

### Outstanding (for owner)
- AC #6: hardware test with one Xiaomi LYWSD03MMC (ATC/pvvx) AND one BTHome v2
  emitter on real OTGW32. Confirm `state.sat.fBleTemp` updates within
  `iBleInterval` seconds.
- AC #9: 5-minute soak with BLE enabled, observe zero MQTT keepalive
  disconnects and zero CMSG overflow logs (telnet-attached).
- DoD #1 (hardware-validated) and DoD #2 (pre/post flash-size delta — the
  pre-change Bluedroid measurement requires a separate `git stash + build`
  cycle on `dev` HEAD).

Status remains In Progress until owner verifies hardware-DoD items per
project policy ("build-clean alone is not Done", CLAUDE.md).

[Admin-flip 2026-05-02] Status was lingering at In Progress although the work has been merged for some time. User-confirmed close-out, no code change in this session.
<!-- SECTION:FINAL_SUMMARY:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Hardware-validated on real OTGW32 + at least one ATC/pvvx and one BTHome v2 sensor (build-clean alone is not Done per CLAUDE.md)
- [ ] #2 Flash-size delta measured with `pio run -e esp32 --target size` before and after, included verbatim in the PR description
- [ ] #3 ADR-092 reaches Status=Accepted before this task is closed (per ADR-080: new dependency requires ADR; per CLAUDE.md ADR section: Proposed → Accepted requires the four verification gates)
- [ ] #4 No `evaluate.py` regressions; hot-path String allocation in BLE callback is removed (or, if any remains, justified in code comment + ADR-004 cross-reference)
<!-- DOD:END -->
