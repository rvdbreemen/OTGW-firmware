---
id: TASK-1684
title: >-
  Remove the OT-Thing standalone controller from OT-Direct (SAT is the only
  controller)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T09:22:00Z'
updated_date: '2026-11-07T09:24:00Z'
labels:
  - otdirect
  - breaking
  - cleanup
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 805000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's decision 2026-11-07 (session, AskUserQuestion): remove, not keep as fallback. The OT-Thing PI room compensation + heating curve + fixed flow + CH hysteresis block (TASK-183, TASK-582, exposed by TASK-933; OTDirect.ino getFlowTemp/loopPiCtrl/loopCHHysteresis, settings otdchmode/otdflowtemp/otdflowmax/otdroomsetpoint/otdgradient/otdexponent/otdoffset/otdkp/otdki/otdkboost/otdhysteresis/otdroomcomp/otdhysteresisenable) only acts in OT-Direct master mode with SAT off. Found via tjfs's 0x2D00 report (TASK-1681): its fixed-flow default writes MsgID 1 = 45.0 C. Side finding: loopCHHysteresis yields to state.sat.bActive instead of satOwnsControlSetpoint() (same gap TASK-1150 closed for the curve). BREAKING: master mode without SAT gets no TSet unless HA/MQTT sends CS=.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The controller code, its settings, REST keys, UI fields and MQTT/HA entities are removed; old settings files load without error (unknown keys ignored)
- [ ] #2 Master mode without SAT behaviour is defined and documented (no TSet write unless CS= arrives), entry in docs/BREAKING_CHANGES.md and CHANGELOG
- [ ] #3 Host tests and evaluate --quick green, combo build, bench .61 in master mode: no MsgID 1 write without SAT/CS, SAT still controls with SAT on
- [ ] #4 App-slot size before/after recorded
<!-- AC:END -->
