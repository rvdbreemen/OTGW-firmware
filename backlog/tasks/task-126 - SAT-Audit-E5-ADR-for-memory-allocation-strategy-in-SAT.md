---
id: TASK-126
title: 'SAT Audit E5: ADR for memory allocation strategy in SAT'
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
  - memory
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR documenting the memory strategy for SAT in the firmware: static buffers, PROGMEM usage, RAM budget and why dynamic allocation is avoided.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR documents RAM budget and allocation
- [x] #2 Rationale for static allocations documented
- [x] #3 ADR published in backlog/decisions/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created docs/adr/ADR-071-sat-memory-allocation-strategy.md documenting:
- All SAT memory is statically allocated (no heap in control loop)
- settings.sat (~80 bytes), state.sat (~350 bytes) follow ADR-051
- File-scope static state per .ino: PID (~40B), cycle history ring 16*20B=320B, weather forecast 24*4B=96B
- PROGMEM manufacturer table: 18 entries * 14B = 252B in flash
- Exception: weather HTTP response uses String for one-off fetch (documented ADR-004 exception)
- climAttrBuf[512] declared static to avoid 512B stack frame in MQTT publish function
- ESP32 BLE fields conditional on #if defined(ESP32)
<!-- SECTION:FINAL_SUMMARY:END -->
