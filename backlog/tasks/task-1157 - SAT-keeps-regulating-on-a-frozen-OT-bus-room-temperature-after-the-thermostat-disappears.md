---
id: TASK-1157
title: >-
  SAT keeps regulating on a frozen OT-bus room temperature after the thermostat
  disappears
status: To Do
assignee: []
created_date: '2026-09-23 18:14'
labels:
  - sat
  - safety
  - bug
dependencies: []
priority: high
ordinal: 289000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTcurrentSystemState.Tr (MsgID 24) is written on decode and never expires. When the thermostat goes quiet, satGetRoomTemp() keeps returning the last Tr forever, so SAT regulates on a frozen value and the invalid-input skip -> safety trip chain never fires. Observed 2026-09-23 on the OTGW32 bench: after a fixture replay ended, SAT ran 10+ min on room=23.4 from the replayed Tr. Found during TASK-1150 validation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tr that has not been refreshed by a thermostat-sourced frame within a bounded window is treated exactly like never-seen Tr (NaN) by satGetRoomTemp()
- [ ] #2 Gateway-originated MsgID 24 traffic (master scheduler, replayed request frames) cannot keep a stale Tr alive
- [ ] #3 Bench A/B: with the thermostat source gone, SAT trips within the existing skip window on the fix build and does not on the previous build
<!-- AC:END -->
