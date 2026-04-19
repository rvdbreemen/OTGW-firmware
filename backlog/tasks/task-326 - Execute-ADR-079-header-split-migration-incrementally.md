---
id: TASK-326
title: Execute ADR-079 header split migration incrementally
status: To Do
assignee: []
created_date: '2026-04-19 07:38'
labels:
  - architecture
  - review-2026-04-18
  - adr-079
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Move section structs from OTGW-firmware.h (1102 lines) into per-component headers per ADR-079. Start with SAT (largest, most self-contained: SATWindowRecord, SATZoneState, SATRuntimeSection, SATSection) in PR 1. Continue with OTDirect in PR 2. One or two sections per PR to keep diffs reviewable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 state_sat.h and settings_sat.h created per ADR-079 conventions; SAT structs moved there; OTGW-firmware.h includes them
- [ ] #2 state_otdirect.h and settings_otdirect.h created; OTDirect structs moved
- [ ] #3 Continue section-by-section until OTGW-firmware.h contains only aggregates + globals + cross-cutting enums
- [ ] #4 Each PR builds cleanly on both ESP8266 and ESP32; OTGWSettings/OTGWState struct sizes remain identical
<!-- AC:END -->
