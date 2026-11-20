---
id: TASK-2043
title: 'SAT: derivative gain factor 0.05 instead of 0.063 (Sergeant D, entry 425)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T05:29:00Z'
updated_date: '2026-11-19T22:31:00Z'
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

2026-11-19 scope settled: entry 427 (Q-425a answered A, Sergeant D 05:42:11 UTC, 'my answer is A', https://discord.com/channels/812969634638725140/1105556725714649128/1572849450299293867; on origin be8166257, opened this session): the Kd factor 0.05 of entry 425 goes into 2.0.0, in the next alpha build, not in beta.382. Agrees with entry 425 (factor 0.05, supersedes 0.063 of entry 279; entry 279's 8400 s stays). No other change: beta.386 (54d46b1b7) stays as committed.

2026-11-19 bench regression (09, .61, 2.0.0-beta.386+54d46b1, OTA app+fs, settings changed none): 31 min 06:51:37-07:22:40 with SAT enabled at 06:51:50 (no boiler: SAT boots, resets the PID and stays waiting; satsimulation off). 31/31 device/info samples HTTP 200, bootcount 1053 start = end (no reboot), 0 panic lines in telnet, freeheap 41260 -> 38972 (min 27808), maxfreeblock 26612 -> 18420. Telnet shows 'SAT: PID integral and raw derivative reset' (satPidResetT:163) and SATBOOT phase=decide as expected. AC3 bench-regression part passed; AC3 stays open for the field check of the warm-up (needs a real boiler) and the developer's evaluate/build evidence. Evidence: 09 scratchpad b386/reg (run.log, info.jsonl, telnet.raw).

2026-11-19 AC3 status (otgw-dev, coordinator's admin round): the bench part holds: evaluate.py --quick 89/0 and the esp32-combo build of beta.386+54d46b1, plus the bench-tester's 31-min regression on .61 (06:51:37-07:22:40, 31/31 device/info 200, bootcount unchanged, satsimulation off). The field part (the warm-up with Kd 0.05 on a real boiler) is still open, so AC3, one checkbox for both, stays unchecked until a tester's SAT run on beta.392+ shows the warm-up.
<!-- SECTION:NOTES:END -->
