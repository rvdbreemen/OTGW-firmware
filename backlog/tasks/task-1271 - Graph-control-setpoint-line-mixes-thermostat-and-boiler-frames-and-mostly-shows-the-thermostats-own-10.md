---
id: TASK-1271
title: >-
  Graph control-setpoint line mixes thermostat and boiler frames and mostly
  shows the thermostat's own 10
status: To Do
assignee: []
created_date: '2026-10-20 18:16'
labels:
  - bug
  - webui
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 401000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. data/v2.js ~464: case 1 sets model.chSet from every write-type MsgID 1 frame regardless of source (T thermostat, R gateway, B, A). Each exchange writes T=10, R=45, B=45, A=10; the A frame comes last, so the graph shows the Honeywell's own TSet 10 most of the time (CSV 11:42-11:51: 17% at 45 while the boiler received 45 throughout). Same pattern for MsgID 16 roomSet (~465), which is the thermostat's TrSet, not SAT's target.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Control-setpoint line shows what the boiler received (B write-ack) in gateway and monitor mode (frame-replay test, red on old code)
- [ ] #2 Room-setpoint line labelled as thermostat setpoint, or SAT target added as its own line
<!-- AC:END -->
