---
id: TASK-106
title: 'SAT Audit B15: BLE / OTGW32 integration'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:29'
labels:
  - audit
  - sat
  - phase-2
  - otgw32
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the BLE integration in Python SAT thermo-nova with C++ firmware (OTGW32). Verify BLE advertising data format, thermostat temperature via BLE, and fallback when BLE is unavailable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BLE advertising data format compared
- [x] #2 Temperature data via BLE verified (encoding, scaling)
- [x] #3 Fallback behavior without BLE verified
- [x] #4 OTGW32-specific code properly abstracted from ESP8266 code
- [x] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Analyze Python SAT sensor input path (outside_sensor, inside_sensor config)
2. Analyze C++ BLE sensor implementation in SATble.ino
3. Check BLE data format parsing vs advertised formats
4. Verify fallback chain, staleness handling
5. Check ESP32/ESP8266 abstraction
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Python SAT has no native BLE support. It uses HA entity references (inside_sensor_entity_id, outside_sensor_entity_id) which can be any HA sensor including BLE sensors managed by HA. The comparison is therefore architectural.

2. C++ SATble.ino implements active BLE passive scanning on ESP32 for:
   - ATC/pvvx custom firmware (UUID 0x181A, 13-14 bytes, temp sint16/100, hum uint16/100, batt uint8)
   - BTHome v2 protocol (UUID 0xFCD2, TLV format, obj 0x02=temp, 0x03=hum, 0x01=batt)

3. BLE data format: ATC temp is sint16 / 100 = 0.01C resolution. BTHome temp obj 0x02 is sint16 / 100. Both correct per their respective specs.

4. Stale handling: BLE_STALE_MS=300000ms (5min) matches SAT_STALE_TEMP_MS. When stale, satGetRoomTemp() falls back: BLE -> MQTT external -> OT bus.

5. MAC filter: Configurable sBleMAC filter. Empty = accept all compatible sensors (first valid sensor used). Multiple sensors tracked (up to SAT_BLE_MAX_SENSORS=4).

6. ESP32/ESP8266 abstraction: BLE code is fully wrapped in #if defined(ESP32). Stub functions would be needed for ESP8266 (stub satBLEGetTemperature returns 0.0f when not ESP32).

7. Scan interval: configurable iBleInterval (default 30s). Python equivalent: HA updates sensor state on BLE advertisement which is continuous.

No functional gaps found. BLE implementation is firmware-only addition with no Python equivalent. AC5: no audit-fix tasks needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B15: BLE / OTGW32 integration

Python SAT has no BLE support (uses HA sensor infrastructure). C++ implements native BLE passive scanning for ATC/pvvx (UUID 0x181A) and BTHome v2 (UUID 0xFCD2) formats with correct sint16/100 temperature decoding.

BLE is ESP32-only, properly wrapped in #if defined(ESP32) at both declaration and call site. Stale timeout (5min) matches SAT_STALE_TEMP_MS. Fallback chain: BLE -> MQTT external -> OT bus temperature.

MAC filtering, multi-sensor support (up to 4 slots), and configurable scan interval are firmware-only extensions beyond Python SAT scope.

No gaps found. No fix tasks required.
<!-- SECTION:FINAL_SUMMARY:END -->
