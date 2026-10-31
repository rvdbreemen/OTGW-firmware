---
id: TASK-1405
title: >-
  SAT classifier: store the control setpoint per tail sample, as thermo-nova
  (entry 36, Q2 A)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T21:44:00Z'
updated_date: '2026-10-31T06:24:00Z'
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
- [x] #2 RAM cost of the per-sample buffer measured and within budget
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

Implemented (sonnet lane wt-1405): per-sample control-setpoint error ring _tail_ctrlErr (int16 hundredths, +360 B static, shares head/count with _tail_samples), _tailCtrlErrPercentiles, continuous branch of _cycleClassify uses it; fFinalSetpoint = thermo-nova _control_setpoint (heating_control.py:255; PWM OFF 10 as MINIMUM_SETPOINT). Tests: classifier 26 OK (+ thermo-nova oracle 2 OK when other-projects present: T1-T5 agree with classifier.py); base FAILED (failures=4: T1,T2,T4,T5). Review (opus): no blocking defects; rings stay in step in all reset paths; fixes applied: non-finite flow left out (tracker.py:268), percentile deviation documented in code. Known deviations: lower-neighbour percentile vs interpolation (helpers.py:187-203), overshoot seconds/shape metrics still per-cycle setpoint (entry 36 did not ask). AC2 RAM: +360 B static by declaration; linker map check after the build.

2026-10-31 (session otgw-firmware-ea): lane wt-v1405 applied on dev as 269178554 (alpha.566). Old-vs-fix: tests/test_sat_cycle_classifier.py with OTGW_SATCYCLES_INO = HEAD SATcycles.ino: FAILED (4 subtests of test_task1405_tail_error_uses_the_per_sample_control_setpoint); fix: 28 OK. All 73 tests/test_*.py OK; evaluate --quick Failed 0. AC#2 RAM measured from the esp32-combo build: 120596 B (alpha.564 build: 120236 B) = +360 B, as designed (180 x int16), total 36.8 % of 327680 B; the percentile sort uses a 360 B stack array (int16_t sorted[SAT_TAIL_SAMPLE_SIZE]). Bench OTGW32 .61, alpha.566, SAT simulation: 7 cycles classified in 12 min (last_cycle_class 1), no restart, free heap 68.9-74.2 KB. Zip msg 1565974455703044166.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The end-of-cycle classifier takes the control-setpoint error per tail sample (_tail_ctrlErr ring, int16 hundredths), as thermo-nova tracker.py:267-271 / classifier.py:71-72 (Sergeant D entry 36, Q2 A); seconds without a control setpoint are left out. Evidence: AC#1 host test old FAIL (4) / fix OK; AC#2 +360 B RAM measured in the build, within budget; bench alpha.566 classifies cycles without fault. Published as 2.0.0-alpha.566+2691785.
<!-- SECTION:FINAL_SUMMARY:END -->
