---
id: TASK-301
title: '[SEC-M1] Restore OTGWSerial on ESP8266 OTA Update.printError paths'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:19'
updated_date: '2026-04-18 21:02'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-ModUpdateServer-impl.h:164,:343 and fallback macro :42-44 changed from OTGWSerial to Serial since v1.3.5. On ESP8266 this writes to the PIC UART, corrupting PIC frames on failed filesystem OTA. ESP32 is fine (different UART).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Platform guard: ESP8266 uses OTGWSerial for Update.printError, ESP32 keeps Serial
- [x] #2 Same fix applied to the #ifndef Debug fallback even if currently dead
- [ ] #3 Manual test on ESP8266: trigger a filesystem OTA failure, PIC link stays clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Applied platform guard on both call sites (lines 164, 343) and on the #ifndef Debug fallback macro block (lines 40-54). ESP8266 routes Update.printError through OTGWSerial; ESP32 keeps Serial (UART0 is free there, OTGWSerial lives on UART1).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTGW-ModUpdateServer-impl.h: wrapped three Serial write sites in #if defined(ESP8266) guards so OTA error output no longer corrupts the PIC UART on ESP8266. Regression against v1.3.5 resolved. ESP32 path unchanged. AC3 (hardware validation) deferred: requires a deliberate OTA failure on a real device.
<!-- SECTION:FINAL_SUMMARY:END -->
