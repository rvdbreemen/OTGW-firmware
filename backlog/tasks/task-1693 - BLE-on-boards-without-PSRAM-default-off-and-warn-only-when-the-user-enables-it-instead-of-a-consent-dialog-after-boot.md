---
id: TASK-1693
title: >-
  BLE on boards without PSRAM: default off and warn only when the user enables
  it (instead of a consent dialog after boot)
status: To Do
assignee: []
created_date: '2026-11-07T12:29:00Z'
labels:
  - needs-maintainer-ok
  - ble
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 808000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs in #alpha-testing 2026-11-07 12:18 UTC (msg 1568600646901305449): instead of warning as soon as a 4 MB / no-PSRAM gateway boots, warn only when the user turns BLE on, and keep it off at boot. Sergeant D 12:20 UTC (msg 1568601041937764453): 'Or even better do not give user the option to enable BLE'. Today: settings.sat.bBleEnable defaults true (SATtypes.h:604), bleActive() needs psramFound() or bBleRiskAck (SATble.ino:636), and v2.js:2884 shows the consent dialog when psram==0, ble_enable and no risk_ack, which a fresh no-PSRAM unit hits on first visit. Changes ADR-169 (PSRAM-aware BLE default with consent gate): Robert's call, a superseding ADR if changed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision by Robert: keep ADR-169, default off on no-PSRAM with a warning on enable, or hide BLE on no-PSRAM boards
<!-- AC:END -->
