---
id: TASK-1656
title: 'SAT: derivative gain factor 0.063 instead of 0.07 (Sergeant D, entry 279)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T11:09:00Z'
updated_date: '2026-11-05T11:31:00Z'
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
- [x] #1 Named constant for the KD factor = 0.063 in SATpid.ino with a comment citing entry 279; kd = round3(0.063 * 8400 * kp)
- [x] #2 Every test/sim that pins 0.07 (test_sat_autogains_removed, test_sat_pid_cadence, test_sat_pid_semantics, scripts/sat_sim_port.py port-equivalence) updated and green; a test fails on the old factor
- [x] #3 docs/sat/firmware-sat-spec.md and sat-comparison.md state the deviation with entry 279; evaluate --quick green
- [ ] #4 TASK-1655 x0.9 result reported to Sergeant D with the numbers
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 (sonnet agent, wt-1645b): SAT_PID_KD_FACTOR = 0.063f in SATpid.ino (entry 279), used at :221; scripts/sat_sim_port.py f32(0.063); tests autogains_removed, pid_cadence, pid_semantics use the factor. Red on HEAD's SATpid.ino: autogains 1 FAIL (2450.196 != 2205.176), pid_cadence 2 FAIL, pid_semantics 1 FAIL, sim_port_equivalence 1 FAIL. Green on the fix: 3 / 31 / 23 / 4 OK; lowload contract 35 OK (skip 1, xfail 2); evaluate 0 failed. Docs: firmware-sat-spec.md (formula, factor table, summary) and sat-comparison.md row 7 'differs (deliberate)' with entry 279. AC4 (1655 x0.9 report to Sergeant D) pending the simulation.
<!-- SECTION:NOTES:END -->
