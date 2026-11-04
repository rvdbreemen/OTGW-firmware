---
id: TASK-247.1
title: >-
  Add SAT conditional debug macros and fill debug gaps in SATcontrol.ino and
  SATcycles.ino
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 09:12'
updated_date: '2026-04-11 09:27'
labels:
  - sat
  - debug
dependencies: []
parent_task_id: TASK-247
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Define SATDebugTf/SATDebugTln/SATDebugf macros gated by state.debug.bSAT (default true, same pattern as OTDebug*). Replace all raw DebugTf/DebugTln calls in SATcontrol.ino and SATcycles.ino with the new macros. Fill in missing debug calls in: cycle classification, flow temp percentile, sustained state tracking, HCR median calculation, rolling window updates, PID steps, setpoint send path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SATDebug* macros defined in SATcontrol.ino, identical pattern to OTDebug* in OTGW-Core.ino
- [x] #2 All existing DebugTf/DebugTln calls in SAT files replaced with SATDebug* equivalents
- [x] #3 satCycleOnFlameChange, satCycleSample, satCycleCheckAutoSwitch, satCycleCountClass get meaningful entry/exit debug lines
- [ ] #4 satHCRAddSample and satHeatingCurveRecommendation log input/output values
- [ ] #5 satControlLoop PID step logs: room/target/error/output/setpoint/mode
- [ ] #6 Build clean
<!-- AC:END -->
