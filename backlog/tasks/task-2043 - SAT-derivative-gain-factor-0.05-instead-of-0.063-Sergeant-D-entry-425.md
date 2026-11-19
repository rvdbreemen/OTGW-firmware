---
id: TASK-2043
title: 'SAT: derivative gain factor 0.05 instead of 0.063 (Sergeant D, entry 425)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T05:29:00Z'
updated_date: '2026-11-19T05:34:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1049000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entry 425 (Sergeant D, 2026-11-19 04:55 UTC, 'change it to 0.05', https://discord.com/channels/812969634638725140/1105556725714649128/1572837659355914303) supersedes the 0.063 factor of entry 279: kd = 0.05 x 8400 x kp = 420 x kp (was 529.2 x kp). The 8400 s of entry 279 stays. thermo-nova pid.py:93 has 0.07 (deliberate deviation). Firmware: SAT_PID_KD_FACTOR (SATpid.ino:28), comments :26-27 and :225-226, the tests that pin 529.2 x kp, and the simulation port (tools/research/sat/sat_sim_port.py _gains), which must follow (port equivalence test). Release scope asked (Q-425a); built for 2.0.0 meanwhile (recommended, no-waiting rule). Follow-up of TASK-1656.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host test calling the real _pidCalculateGains pins kd = 420 x kp: red on the 0.063 code, green on the fix
- [x] #2 Comments, sim port and every test pinning 529.2 x kp follow 0.05; test_sat_sim_port_equivalence green
- [ ] #3 evaluate.py --quick no new FAIL; esp32-combo build; bench regression (no SAT boiler needed, field check of the warm-up)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 implemented in lane-2026 (wt-2026) as beta.386, on top of TASK-2026 (383-385); scope 2.0.0 per the recommended option while Q-425a is open (no-waiting rule). AC1 red: test constants set to 0.05 against the 0.063 firmware (SATpid.ino:28): 4 failed (test_sat_autogains_removed::test_gains_match_the_automatic_formula, test_sat_pid_cadence::test_m_first_output_with_pid_terms_has_d_zero_then_d and test_n_late_restore_gives_d_zero_on_the_next_output, test_sat_pid_semantics::HeldDerivative); these compile the real SATpid.ino. Green on SAT_PID_KD_FACTOR 0.05f. AC2: comments SATpid.ino:26-27/:226, sim port _gains 0.05, test comments, firmware-sat-spec.md (5 places); 8 SAT/port test files 160 passed incl. test_sat_sim_port_equivalence (4/4) and the 1655/1784 studies (they inject explicit factors, independent of the port default). evaluate.py --quick 89 passed 0 failed. AC3 pending: build in the slot after the 08:00 beta, bench regression.
<!-- SECTION:NOTES:END -->
