---
id: TASK-3050
title: >-
  SAT: PID and Summer Simmer roundings like thermo-nova (entries 482 and 490):
  ki/kp/kd 6 decimals, output 1 decimal, Simmer index 1 decimal, ties like
  Python
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T07:07:00Z'
updated_date: '2026-11-25T08:45:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1153000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, entries 482 (Ki to 6 decimals, otherwise Ki would be 0) and 490 (all roundings like thermo-nova, answer B; amends entry 95 Q28a). SATpid.ino: kp, ki, kd computed and rounded in double to 6 decimals (pid.py:71, :82, :93), P/I/D/raw derivative stay 3 (pid.py:101-119), PID output 1 decimal incl. the curve-only output (pid.py:132); SATcontrol.ino satCalcSimmerIndex rounds every return path to 1 decimal (summer_simmer.py:38) and the comment near :2876 now says 22 C / 70 % = 26.9 C (verified by running the real function: 26.941597). New helper satRoundHalfEvenDecimals() = Python round() on the exact binary value (fma). float32 limit: kd about 1e4 keeps about 3 decimals when stored as float. Release 2.0.0 if the bench confirms, else 2.1.0 (entry 490). Absorbs the Simmer part of the planned TASK-3043.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host tests on the real functions with Python-computed expectations: red on the old source, green on the fix (tests/test_sat_pid_rounding.py, test_sat_simmer_rounding.py, test_sat_autogains_removed.py, test_sat_pid_semantics.py)
- [ ] #2 evaluate.py --quick no new FAIL; full tests green
- [ ] #3 Bench check (otgw-bench-tester, check_pid_formulas.py): REST sat/status kp, ki, kd, pid_p/i/d, pid_output show the new precision with unchanged field names
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 implemented by a Sonnet lane, reviewed by otgw-dev. RED on the unchanged source: test_sat_pid_rounding (setUpClass error + 1 fail), test_sat_simmer_rounding (9 fails), test_sat_autogains_removed (5), test_sat_pid_semantics (16), test_sat_pid_cadence (3). GREEN on the beta tree: all of those plus test_sat_pid_persist, test_sat_humidity_source, test_sat_kd_005_sim OK. Tie helper vs Python round(): 45000 random/near-tie values (1, 3, 6 decimals) identical; 45.25 -> 45.2, 2.675 -> 2.67, 0.125 -> 0.12. float32 storage: kp worst error 2.9e-6, ki 1.9e-10, kd 4.5e-4 (about 3 decimals at kd ~1e4). Display decimals of sat/kp (4) and sat/kd (2) unchanged (display only, no test pins them). Not verified: ESP32-S3 build with fma(); bench. Zone PID ki (satZonePidAdvance) untouched, still unrounded kp/8400. Q28b: kp <= 0.0042 gives ki 0 by rounding.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DRAFT 2026-11-25 (bench item pending, task NOT Done). Change (entries 482 and 490, amending entry 95 Q28a): kp, ki, kd computed and rounded in double to 6 decimals like pid.py:71/:82/:93; P, I, D and raw derivative stay at 3 (pid.py:101-119); PID output at 1 decimal incl. the curve-only output (pid.py:132); Summer Simmer index at 1 decimal on every return path (summer_simmer.py:38) and the wrong comment fixed (22 C / 70 % = 26.941597, shown as 26.9, verified by running the real function); new satRoundHalfEvenDecimals() = Python round() on the exact binary value (fma), 45000 random/near-tie values identical to Python at 1, 3 and 6 decimals; 45.25 -> 45.2, 2.675 -> 2.67, 0.125 -> 0.12. Evidence per AC: AC1 RED on the unchanged source: test_sat_pid_rounding (error + 1 fail), test_sat_simmer_rounding 9, test_sat_autogains_removed 5, test_sat_pid_semantics 16, test_sat_pid_cadence 3; GREEN on the fix plus test_sat_pid_persist, humidity_source, kd_005_sim. AC2 evaluate --quick 89/0/0 on R; affected-tests pass 416 of 422, the 5 red pre-existing. Test pins moved with the new rules: room_loss (ki 6 dec, output 1 dec), sim_end_restore marker, closedloop H4 seed 2 (the 1-decimal output hides the seed-1 mutant). Limits: float32 storage keeps kd (about 1e4) to about 3 decimals (error 4.5e-4), kp 2.9e-6, ki 1.9e-10; sat/kp and sat/kd display decimals (4 and 2) unchanged; zone PID ki unchanged. PENDING: AC3 bench (check_pid_formulas.py: REST sat/status kp ki kd pid_p/i/d pid_output, field names unchanged). Not verified: fma() runtime on the ESP (it builds and links), real boiler.
<!-- SECTION:FINAL_SUMMARY:END -->
