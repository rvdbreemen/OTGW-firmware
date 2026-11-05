---
id: TASK-1654
title: 'Bench: second OTGW32 as an OpenTherm boiler simulator for SAT wire-level tests'
status: To Do
assignee: []
created_date: '2026-11-05T07:00:00Z'
labels:
  - sat
  - bench
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 778000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05 (TASK-1412.20 R-1): a second OTGW32 runs the vendored OpenTherm library in slave mode as a programmable boiler, wired to the bench unit's boiler side, so ID25/ID28 Data-Invalid, Unknown-DataID, no-answer, ignition delays and DHW can be driven on the wire (replay cannot reach the flow/return feeds).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Simulator firmware or sketch answers MsgID 0/1/3/17/25/28 with scripted values and faults, driven by a scenario file
- [ ] #2 Bench unit sees the simulator as a boiler on the wire (B frames from the bus, not replay), shown in a capture
- [ ] #3 TASK-1412.20 SB05-SB09 and SB11 run on the wire with the expected SAT reactions recorded
<!-- AC:END -->
