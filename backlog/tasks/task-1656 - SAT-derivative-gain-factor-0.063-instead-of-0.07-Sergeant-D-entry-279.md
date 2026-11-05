---
id: TASK-1656
title: 'SAT: derivative gain factor 0.063 instead of 0.07 (Sergeant D, entry 279)'
status: To Do
assignee: []
created_date: '2026-11-05T11:09:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 780000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-05 (#dev-sat-mqtt 1567827743976980561): 'change the value 0.07 to 0.063' (KD -10%), with the design intent 'SAT is designed to reach the deadband in 8400 seconds during morning warm-up' (1567828260404863027). Firmware SATpid.ino:221 kd = round3(0.07f * 8400 * kp) -> 0.063f; deliberate deviation from thermo-nova pid.py (0.07). TASK-1655 simulation x0.9 is the evidence run.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Named constant for the KD factor = 0.063 in SATpid.ino with a comment citing entry 279; kd = round3(0.063 * 8400 * kp)
- [ ] #2 Every test/sim that pins 0.07 (test_sat_autogains_removed, test_sat_pid_cadence, test_sat_pid_semantics, scripts/sat_sim_port.py port-equivalence) updated and green; a test fails on the old factor
- [ ] #3 docs/sat/firmware-sat-spec.md and sat-comparison.md state the deviation with entry 279; evaluate --quick green
- [ ] #4 TASK-1655 x0.9 result reported to Sergeant D with the numbers
<!-- AC:END -->
