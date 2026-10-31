---
id: TASK-1522
title: >-
  Monitor graph: control setpoint sample can show the thermostat's request
  instead of the value the boiler got
status: To Do
assignee: []
created_date: '2026-10-31T09:46:00Z'
labels:
  - alpha
  - bug
  - graph
dependencies: []
priority: medium
ordinal: 678000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter alpha.567 capture 20261031-092435 (Discord #alpha-testing msg 1566024293173952574): graph chSet 29.7 at 09:47:10 for one 5 s slot, while every MsgID 1 the boiler received was 10.0 (raw frames: T 90011DB3 replaced by R 10010A00, B D0010A00). graphHistoryTick() samples OTcurrentSystemState.TSet (graphHistoryStuff.ino:60); find which path writes the T-frame value into TSet (live frame decode order T before R/B, or the PS summary) and sample the value the boiler acknowledged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test or replay of the frame sequence T 90011DB3 / R 10010A00 / B D0010A00 shows the old code storing 29.7 in TSet (or in the graph slot) and the fix storing 10.0
- [ ] #2 Graph chSet and REST controlsetpoint show the boiler-side value when the gateway overrides MsgID 1
- [ ] #3 Bench or capture evidence attached
<!-- AC:END -->
