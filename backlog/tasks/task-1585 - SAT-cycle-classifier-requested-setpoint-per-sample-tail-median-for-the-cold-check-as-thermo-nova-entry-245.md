---
id: TASK-1585
title: >-
  SAT cycle classifier: requested setpoint per sample, tail median for the cold
  check, as thermo-nova (entry 245)
status: To Do
assignee: []
created_date: '2026-11-03T06:42:00Z'
updated_date: '2026-11-03T07:04:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 730000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 245 (Q-PARITY-5 A, 2026-11-03): record the PID requested setpoint per classifier sample and use its tail p50 as the effective setpoint for the underheat cold check (thermo-nova cycles/tracker.py:273-277, classifier.py:81-86), instead of one snapshot at flame-off (SATcycles.ino:918). Found by TASK-1412.14. Expected RAM: +360 B (int16 per tail slot).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real SATcycles.ino: a burn whose requested setpoint changes in the last 3 min near the cold line gets thermo-nova's verdict; the parity scenarios requested_raised_inside_tail_pwm, requested_missing_last_third_pwm and requested_lowered_below_cold_continuous of tests/test_sat_classifier_parity.py agree (removed from the DIFFERENCES list); red on the old code
- [ ] #2 RAM delta measured from the build (size -A)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead in wt-next): new int16 ring _tail_reqSp[180] (+360 B static; tail rings 1080 -> 1440 B), same indices as _tail_samples so the TASK-1584 warm-up trim applies. _tailReqMetrics() gives the tail p50 of the requested setpoint (cold check) and the per-sample tail p90 of flow minus requested (PWM underheat), as thermo-nova classifier.py:45/81-86, tracker.py:273-277. Lead decision: the per-sample p90 is inside entry 245 ('per sample') and entry 55 ('match classifier.py exactly'). Fallback unchanged (control setpoint at flame-on; Q-PARITY-6 refuted). Side effect: the unreachable NaN-flow scenario (Q-PARITY-9, refuted) now agrees too. fCycleReqSetpointError now carries reqErrP90. Tests: classifier 31 OK (old 4 FAIL), parity 12 OK (xfail gate), the three Q-PARITY-5 scenarios now agree in both configs. Stack: ~720 B transient in the flame-off path, not measured on the device. AC2 (RAM from the build) after the build.
<!-- SECTION:NOTES:END -->
