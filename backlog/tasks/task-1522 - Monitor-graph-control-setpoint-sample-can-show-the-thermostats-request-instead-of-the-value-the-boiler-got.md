---
id: TASK-1522
title: >-
  Monitor graph: control setpoint sample can show the thermostat's request
  instead of the value the boiler got
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T09:46:00Z'
updated_date: '2026-10-31T12:32:00Z'
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
- [x] #1 Host test or replay of the frame sequence T 90011DB3 / R 10010A00 / B D0010A00 shows the old code storing 29.7 in TSet (or in the graph slot) and the fix storing 10.0
- [x] #2 Graph chSet and REST controlsetpoint show the boiler-side value when the gateway overrides MsgID 1
- [ ] #3 Bench or capture evidence attached
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause: not the firmware. processOT flags the T (09:47:14.888) as gateway-substituted (<ignored>) and the canonical TSet / device graph history stays 10.0. The exported CSV comes from the browser graph (v2.js applyFrame, case 1): a T frame counted at once when no R for MsgID 1 came in 30 s (thermostat and SAT both on 10.0, so the PIC sent no R); the R arrived 90 ms later, across the 5 s slot boundary, so that slot kept 29.7. Fix: an accepted T is provisional; it counts at a B Write-Ack for MsgID 1 with no R in between, or at the first frame >= 500 ms later; an R cancels it. Test tests/webui/graph-control-setpoint-source.test.mjs case i (gruijter sequence) red on HEAD v2.js, all 11 green on the fix; all 30 webui test files green; evaluate --quick Failed 0.
<!-- SECTION:NOTES:END -->
