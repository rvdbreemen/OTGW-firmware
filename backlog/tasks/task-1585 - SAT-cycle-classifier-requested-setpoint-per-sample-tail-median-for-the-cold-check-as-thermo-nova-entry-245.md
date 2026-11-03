---
id: TASK-1585
title: >-
  SAT cycle classifier: requested setpoint per sample, tail median for the cold
  check, as thermo-nova (entry 245)
status: To Do
assignee: []
created_date: '2026-11-03T06:42:00Z'
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
- [ ] #1 Host test on the real SATcycles.ino: a burn whose requested setpoint changes in the last 3 min near the cold line gets thermo-nova's verdict; the parity scenarios requested_raised_inside_tail_pwm, requested_missing_last_third_pwm and requested_lowered_below_cold_continuous of tests/test_sat_classifier_parity.py agree (removed from the DIFFERENCES list); red on the old code
- [ ] #2 RAM delta measured from the build (size -A)
<!-- AC:END -->
