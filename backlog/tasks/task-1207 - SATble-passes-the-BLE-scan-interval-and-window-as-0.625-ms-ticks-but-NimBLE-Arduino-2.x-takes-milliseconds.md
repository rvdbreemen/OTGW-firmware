---
id: TASK-1207
title: >-
  SATble passes the BLE scan interval and window as 0.625 ms ticks, but
  NimBLE-Arduino 2.x takes milliseconds
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 17:55'
updated_date: '2026-10-04 09:16'
labels:
  - ble
  - sat
dependencies: []
priority: low
ordinal: 329000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1199 verifier. NimBLEScan.cpp:494 and :502 convert milliseconds to ticks (itvl = intervalMs * 16 / 10). SATble.ino:68, :71-72 and :644-646 pass 160 and 80 described as ticks, so the real scan runs with a 160 ms interval and an 80 ms window instead of the 100 ms and 50 ms the comments intend. The duty cycle is 50 % either way.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The intended interval and window are decided and passed in milliseconds; the comments describe the real unit
- [x] #2 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision: keep the scan at what it really ran with in the field, 160 ms interval and 80 ms window (50 % duty), and fix the unit. The constants are renamed BLE_SCAN_INTERVAL_MS / BLE_SCAN_WINDOW_MS (values unchanged) and the comments describe milliseconds. Verified in the library: .pio/libdeps/esp32-combo/NimBLE-Arduino/src/NimBLEScan.cpp:494 and :502 compute (ms * 16) / 10, and NimBLEScan.h declares setInterval(uint16_t intervalMs). Why not switch to the 100/50 ms the old comments intended: no evidence asks for it, and it would change the radio timing next to WiFi. The TASK-1199 bench run (2026-10-04) saw 4 valid sensors at 160/80 ms. No behaviour change: the arguments passed to NimBLE are identical.

Bench 2026-10-04, alpha.412+8ad028c with BLE consent on: scan active, 4 valid roster sensors, internal_free 31.8 KB, maxfreeblock 28.7 KB (same as the TASK-1199 measurement). Builds: esp32, esp32-classic, esp32-combo SUCCESS (alpha.412); evaluate.py --quick 0 failures.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SATble's BLE scan constants carry the unit NimBLE-Arduino 2.x really takes. They were documented as 0.625 ms ticks, but NimBLEScan::setInterval/setWindow take milliseconds (NimBLEScan.cpp:494/502: ms * 16 / 10), so the scan has always run at a 160 ms interval and an 80 ms window. Decision: keep those field-proven values (50 % duty) and rename the constants BLE_SCAN_INTERVAL_MS / BLE_SCAN_WINDOW_MS with matching comments. No behaviour change. Evidence: library source read; bench alpha.412 with BLE active reports 4 valid sensors; builds for 3 envs green, evaluate clean.
<!-- SECTION:FINAL_SUMMARY:END -->
