---
id: TASK-757
title: >-
  feat(oled): decouple OLED from HAS_OLED_CAPABLE — always compile,
  runtime-detect
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 10:10'
updated_date: '2026-05-29 10:47'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Per Robert: an OLED should work whenever it is physically connected, independent of the board's compile-time hardware description. Runtime detection already exists (probeOLED() I2C-probe at 0x3C -> oledPresent; graceful when absent). Decouple OLED *compilation* from the HAS_OLED_CAPABLE board flag so the display works on any board (incl. ESP8266 OTGW-classic with an OLED on the shared I2C bus — the bus is already up for the external watchdog). SSD1306Ascii already links for both targets (verified in .pio). boards.h is NOT touched (avoids contention with the concurrent ESP-abstraction session); HAS_OLED_CAPABLE remains as an informational 'onboard OLED' hint. The ESP32-only button ISR (#if defined(ESP32) in OLED.ino) is a pre-existing abstraction exception left as-is here and tracked for shim-migration in a follow-up. Documented in ADR-114.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OLED.ino compiles unconditionally (outer HAS_OLED_CAPABLE gate removed); runtime oledPresent is the source of truth
- [ ] #2 state.hw.bOLEDPresent always present (Hardwaretypes.h ungated)
- [ ] #3 setup() inits I2C+OLED on all boards; device-info reports oledpresent on all boards
- [ ] #4 ESP32-S3 AND ESP8266 both build green (python build.py); ESP8266 image size delta recorded
- [ ] #5 evaluate.py shows no NEW abstraction violations (button #if defined(ESP32) is pre-existing, unchanged)
- [ ] #6 ADR-114 written documenting the runtime-detection-over-compile-gate decision
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OLED decoupled from HAS_OLED_CAPABLE: always compiled + runtime-detected (probeOLED 0x3C). ADR-114. Both targets build green (ESP8266 RAM 88%/Flash 83.5%; ESP32-S3 Flash 95.2%). Shipped alpha.92. boards.h untouched; button #if defined(ESP32) tracked in TASK-758.
<!-- SECTION:FINAL_SUMMARY:END -->
