---
id: TASK-128
title: 'SAT Audit E7: ADR for OTGW32 and ESP8266 compatibility layer'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:55'
updated_date: '2026-04-09 05:25'
labels:
  - audit
  - sat
  - phase-5
  - adr
  - otgw32
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR documenting the compatibility strategy for OTGW32 and ESP8266 in SAT code: abstraction layers, build conditionals and shared interfaces.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR describes both target platforms and their differences
- [x] #2 Chosen abstraction strategy documented
- [x] #3 ADR published in backlog/decisions/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created docs/adr/ADR-073-sat-platform-compatibility.md documenting:
- Platform differences: BLE (ESP32 only), RAM, RTOS vs cooperative, OT data source
- Strategy: compile-time #if defined(ESP32) guards for BLE only
- Core algorithms (heating curve, PID, cycles, OPV calibration) have zero platform-specific code
- SATble.ino: entirely wrapped in #if defined(ESP32)
- OTcurrentSystemState is platform-agnostic thanks to frame bridge pattern (ADR-087)
- SATStateSection has conditional BLE fields
- weatherFetch uses WiFiClient/HTTPClient abstracted by platform.h (ADR-061)
- climAttrBuf[512] static on both platforms; constrains ESP8266 stack, harmless on ESP32
<!-- SECTION:FINAL_SUMMARY:END -->
