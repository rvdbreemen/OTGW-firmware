---
id: TASK-1784
title: >-
  SAT: simulate KD factor 0.05 against today's 0.063 for the warm-up (Sergeant D
  order 2026-11-10)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T18:37:00Z'
updated_date: '2026-11-10T19:08:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 879000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #dev-sat-mqtt 2026-11-10 19:33 UTC, msg 1569781945460727873 (relayed by the coordinator, not seen first-hand): 'run a simulation with derivative coefficient 0.5 instead of 0.63'. Read as the kd factor SAT_PID_KD_FACTOR (SATpid.ino:28, 0.063 per entry 279) to 0.05, ratio 0.794 (0.5/0.63 gives the same ratio). Reverses entry 280 (no more simulation). Same plant classes, grid and pre-registered thresholds as TASK-1655 (docs/sat/sim-1655/thresholds.md), baseline now 0.063.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Pre-registration committed before the first run: same thresholds and rule as TASK-1655, baseline 0.063, variant 0.05
- [x] #2 Fairness check: the 0.063 baseline reproduces the committed TASK-1655 x0.9 rows exactly (host test)
- [x] #3 Post text handed to 97 for #dev-sat-mqtt; Sergeant D's answer recorded as a numbered entry
- [ ] #4 Result per plant class 0.05 vs 0.063 (t03, t01, overshoot, starts, guards) plus a chart; report docs/sat/sim-1784/
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 030509: pre-registration docs/sat/sim-1784/thresholds.md (TASK-1655 rules, baseline 0.063, variant 0.05). Wrapper tools/research/sat/sat_kd_005_sim.py imports and patches the 1655 module (file unchanged). tests/test_sat_kd_005_sim.py 3/3 OK in 1.0 s; red check with baseline K forced to 0.07: 2 failures (kd 14112.0 != 12700.8; warm-up t03 142.0 != 139.7), so the fairness test detects a wrong baseline. Entry 297 recorded (relayed, not seen first-hand). Grid run waits for 4c's beta.172 build and the coordinator's go.

2026-11-10 030509: grid run 19:44:46-19:52 local (1152 runs, commit charge 71-74 %); merge: 576 baseline runs equal the committed 1655 x0.9 runs (0 mismatches); validity check passed; 0.05 passes 0 of 8 classes (B 0.6-3.4 %), no guard fails because of 0.05; recommendation by the rule: keep 0.063. Results f32f54a61. Post text with options A/B/C handed to 97 (HOLD until push). Time note: the order is 18:33 UTC by its message id (the description says 19:33, local time).
<!-- SECTION:NOTES:END -->
