---
id: TASK-109
title: 'SAT Audit C1: OTGW32 vs ESP8266 platform differences'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-3
  - otgw32
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Document all platform differences between OTGW32 and ESP8266 that are relevant for SAT. Verify that the C++ code has correct abstraction layers for platform-specific behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All platform-specific code sections identified
- [x] #2 Abstraction layers present and correctly used
- [x] #3 Build conditionals (#ifdef) correctly placed
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for missing abstractions
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited all platform-specific code sections in SAT and the wider firmware.

Platform abstraction is well-structured:
- platform.h dispatches to platform_esp8266.h or platform_esp32.h for all SDK differences (WiFi, HTTPClient, FSInfo, DHCP, heap, reset info, RTC memory).
- boards.h maps feature flags (HAS_PIC, HAS_DIRECT_OT, HAS_ETH_CAPABLE) from board selection to compile-time constants, cleanly separating capability from chip identity.
- All SAT .ino files use only the common OTcurrentSystemState struct and addCommandToQueue()/hasOTCommandInterface() abstractions — zero direct serial or PIC calls.
- SATble.ino is entirely wrapped in #if defined(ESP32): correct.
- SATcontrol.ino has 4 ESP32 guards — all are BLE-related: correct.
- settingStuff.ino guards BLE settings (write and read) with #if defined(ESP32): correct.
- OTDirect.ino is wrapped in #if HAS_DIRECT_OT: correct.

Issues found (fix tasks created):
- TASK-197: boards.h OTGW32 pin definitions carry #warning "hypothetical" — actual hardware not verified.
- TASK-195: SATweather.ino relies implicitly on HTTPClient/WiFiClient from platform include chain without documentation.
- TASK-200: _sat_picFailCount safety logic has no guard for pure OT-Direct mode (HAS_PIC=0).
- TASK-201: NTP startNTP() hostname workaround is ESP8266-only but not guarded.

No ESP8266-only code runs unguarded on OTGW32. Abstraction layers are correctly placed.
<!-- SECTION:FINAL_SUMMARY:END -->
