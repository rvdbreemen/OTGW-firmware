---
id: TASK-3050
title: >-
  SAT: PID and Summer Simmer roundings like thermo-nova (entries 482 and 490):
  ki/kp/kd 6 decimals, output 1 decimal, Simmer index 1 decimal, ties like
  Python
status: Done
assignee:
  - '@claude'
created_date: '2026-11-25T07:07:00Z'
updated_date: '2026-11-27T05:48:00Z'
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
- [x] #2 evaluate.py --quick no new FAIL; full tests green
- [x] #3 Bench check (otgw-bench-tester, check_pid_formulas.py): REST sat/status kp, ki, kd, pid_p/i/d, pid_output show the new precision with unchanged field names
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 implemented by a Sonnet lane, reviewed by otgw-dev. RED on the unchanged source: test_sat_pid_rounding (setUpClass error + 1 fail), test_sat_simmer_rounding (9 fails), test_sat_autogains_removed (5), test_sat_pid_semantics (16), test_sat_pid_cadence (3). GREEN on the beta tree: all of those plus test_sat_pid_persist, test_sat_humidity_source, test_sat_kd_005_sim OK. Tie helper vs Python round(): 45000 random/near-tie values (1, 3, 6 decimals) identical; 45.25 -> 45.2, 2.675 -> 2.67, 0.125 -> 0.12. float32 storage: kp worst error 2.9e-6, ki 1.9e-10, kd 4.5e-4 (about 3 decimals at kd ~1e4). Display decimals of sat/kp (4) and sat/kd (2) unchanged (display only, no test pins them). Not verified: ESP32-S3 build with fma(); bench. Zone PID ki (satZonePidAdvance) untouched, still unrounded kp/8400. Q28b: kp <= 0.0042 gives ki 0 by rounding.

2026-11-25 FULL TESTS on origin/dev 686e33438 (= beta.429 build tree), after 'npm ci --prefix tests/webui': python scripts/run_tests.py --webui -j 4 --timeout 900: 475 files, 469 passed, 5 failed, 0 timeout, 1 skipped (1584.8 s). The 5 reds, none caused by this task: (1) tests/test_manual_sat_commands.py and (2) tests/test_sat_mqtt_state.py: MQTT command room_source (SATroomsource) missing in MANUAL.md and manuals; present in MQTTstuff.ino and absent from the manual already at 21e5ecb01, before beta.428 (TASK-3011/3015 lane, docs owner); (3) tests/test_sat_narrate_fits.py: SATble.ino:851 narration with an unbounded %s, from TASK-3011 (5076410fd), predates beta.428; (4) tests/test_open_questions_status.py: live queue lacks Q-466a while the table lists it; red with the older decisions pointer 71698ea too, decisions-repo bookkeeping (sat_open_questions.py --update); (5) tests/webui/settings-help.test.mjs: satroomsource help text 304 chars (limit 300), otgw-webui fixes. tests/webui/v2-undefined-names passes with acorn installed (a81749c8a). Judged against this: 'full tests green' holds except these 5 named pre-existing reds.

2026-11-27 closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed. Status closed from backlog evidence; no new test run in this step.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 and AC2 were ticked earlier (host tests with Python-computed expectations, red on the old code; commit 5534647d8, first tag beta.428). AC3: bench-63/2026-11-25_beta-candidate/pid-run (INDEX row 4): ki, kd, P and output with the new precision on 163 of 163 samples, field names unchanged. Per Robert's waiver: closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed.
<!-- SECTION:FINAL_SUMMARY:END -->
