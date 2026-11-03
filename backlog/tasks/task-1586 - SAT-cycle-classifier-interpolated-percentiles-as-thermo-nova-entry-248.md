---
id: TASK-1586
title: 'SAT cycle classifier: interpolated percentiles, as thermo-nova (entry 248)'
status: To Do
assignee: []
created_date: '2026-11-03T07:52:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 731000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 248 (Q-PARITY-1 A, 2026-11-03): the tail percentiles (p10/p50/p90, and the control-error and requested-setpoint percentiles) interpolate between the two neighbouring sorted values as thermo-nova helpers.py:185-202 (percentile_interpolated), instead of the lower neighbour (SATcycles.ino:610, :640, and the TASK-1585 helper).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real SATcycles.ino: percentiles equal helpers.py percentile_interpolated on the same samples; the interp_* parity scenarios of tests/test_sat_classifier_parity.py agree (removed from the DIFFERENCES list); red on the old code
- [ ] #2 No RAM change; int16 percentiles keep their hundredths resolution
<!-- AC:END -->
