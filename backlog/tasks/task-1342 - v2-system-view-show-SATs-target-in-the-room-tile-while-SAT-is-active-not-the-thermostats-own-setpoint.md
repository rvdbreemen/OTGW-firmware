---
id: TASK-1342
title: >-
  v2 system view: show SAT's target in the room tile while SAT is active (not
  the thermostat's own setpoint)
status: To Do
assignee: []
created_date: '2026-10-24T09:26:00Z'
labels:
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 465000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-24 09:08 UTC (alpha.487, PIC Classic, Honeywell Round): the system view room tile shows 'set 15.0' (the thermostat's own TrSet, MsgID 16) while SAT controls on its target 19.5 (flame on). His capture (20261024-110022) shows TC=19.5 pushed at 11:01:17, the PIC answering TrOverride 19.50 until 11:04:29 and then 0 (the Round refuses the remote override, PIC cancels after its attempts); with TASK-1272 (entry 16) SAT no longer re-pushes. While SAT is the active controller the room tile should show SAT's target as the main setpoint and the thermostat's own setpoint as secondary (e.g. 'thermostat 15.0').
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With SAT active the room tile shows SAT's target (sat/status target_temp) as 'set'; the thermostat's TrSet is shown as secondary; without SAT unchanged (screenshot before/after in a webui test with a fake device)
<!-- AC:END -->
