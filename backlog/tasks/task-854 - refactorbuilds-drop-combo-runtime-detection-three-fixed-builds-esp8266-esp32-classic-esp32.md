---
id: TASK-854
title: >-
  refactor(builds): drop combo runtime detection - three fixed builds (esp8266,
  esp32-classic, esp32)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-10 21:45'
updated_date: '2026-06-10 22:31'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field result: combo autodetect + combo build does not work correctly. Maintainer decision: remove runtime hardware detection entirely. Replace the esp32-combo build with a compile-time esp32-classic build (S3 in OTGW Classic socket, PIC, D1-mini footprint pins) alongside the existing esp32 (OTGW32, OTDirect) and esp8266 builds. Supersede ADR-125.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 boards.h: BOARD_NODOSHOP_ESP32_CLASSIC with compile-time HAS_PIC=1/HAS_DIRECT_OT=0 and Classic footprint pins; combo board + HAS_RUNTIME_HW_DETECT removed
- [x] #2 platformio.ini + build.py: esp32-classic env/asset set replaces esp32-combo
- [x] #3 All HAS_RUNTIME_HW_DETECT code paths removed (detection block, bootdetect log, iBoardMode setting, activeI2c helpers, runtime hardwareTypeName)
- [x] #4 ADR superseding ADR-125 documents the reversal
- [x] #5 All three builds green + evaluator green
- [x] #6 WiFi-portal-first boot order (TASK-853) preserved
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: combo board + HAS_RUNTIME_HW_DETECT machinery fully removed (detection block, bootdetect log, iBoardMode setting incl. settingStuff sites, activeI2c helpers, runtime hardwareTypeName). New standalone BOARD_NODOSHOP_ESP32_CLASSIC in boards.h (PIC RST=12, UART 43/44, I2C 36/35, button 18, LED 16/4; HAS_PIC_WATCHDOG=1 so the Classic 0x26 watchdog is fed). platformio env esp32-classic (mirrors esp8266: lib_ignore OpenTherm, OTDirect.ino excluded, type stubs, USB-CDC console). build.py target esp32-classic. ADR-126 written, ADR-125 marked Superseded. Docs (PINOUT.md) reframed. Review-agent findings applied: Wire.begin before WatchDogEnabled(0) (0x26 disarm on wrong pins on S3), BOARD_NAME macro (fixes Unknown-board in HA + removes 2 abstraction violations), HAS_LEDC_LED flag (classic gets PWM LEDs, removes another violation), NVS portal-counter writes only on change. ESP_ABSTRACTION_BASELINE 4->1. WiFi-portal-first boot order preserved for all boards.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Combo runtime detection removed; three fixed builds: esp8266 (Classic D1 mini), esp32 (OTGW32), esp32-classic (Classic + LOLIN S3 Mini, PIC, 0x26 watchdog fed). ADR-126 supersedes ADR-125. Review fixes: Wire.begin before watchdog disarm, BOARD_NAME, HAS_LEDC_LED, NVS write guards. ESP_ABSTRACTION_BASELINE 4->1. All three builds green, evaluator green.
<!-- SECTION:FINAL_SUMMARY:END -->
