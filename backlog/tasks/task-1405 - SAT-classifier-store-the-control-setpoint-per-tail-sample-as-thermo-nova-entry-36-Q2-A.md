---
id: TASK-1405
title: >-
  SAT classifier: store the control setpoint per tail sample, as thermo-nova
  (entry 36, Q2 A)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T21:44:00Z'
updated_date: '2026-10-30T19:43:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 543000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 36 (Q2 A): the end-of-cycle mode switch waits until the classifier uses a per-sample control setpoint in the tail, as thermo-nova does, instead of the per-cycle _cycle_setpointAtStart (SATcycles.ino ~740/911/920/947 per the 2026-10-25 audit). Prerequisite for TASK-1331, alongside TASK-1387.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Tail control-setpoint error p50/p90 computed from per-sample setpoints (host test red on HEAD: a setpoint change mid-cycle changes the classification)
- [ ] #2 RAM cost of the per-sample buffer measured and within budget
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

Implemented (sonnet lane wt-1405): per-sample control-setpoint error ring _tail_ctrlErr (int16 hundredths, +360 B static, shares head/count with _tail_samples), _tailCtrlErrPercentiles, continuous branch of _cycleClassify uses it; fFinalSetpoint = thermo-nova _control_setpoint (heating_control.py:255; PWM OFF 10 as MINIMUM_SETPOINT). Tests: classifier 26 OK (+ thermo-nova oracle 2 OK when other-projects present: T1-T5 agree with classifier.py); base FAILED (failures=4: T1,T2,T4,T5). Review (opus): no blocking defects; rings stay in step in all reset paths; fixes applied: non-finite flow left out (tracker.py:268), percentile deviation documented in code. Known deviations: lower-neighbour percentile vs interpolation (helpers.py:187-203), overshoot seconds/shape metrics still per-cycle setpoint (entry 36 did not ask). AC2 RAM: +360 B static by declaration; linker map check after the build.
<!-- SECTION:NOTES:END -->
