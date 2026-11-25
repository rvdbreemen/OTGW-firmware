---
id: TASK-3041
title: >-
  SAT: reduce the Kd factor from 0.05 to 0.04 (entry 477, amends entries
  425/427)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T05:11:00Z'
updated_date: '2026-11-25T11:33:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1149000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #dev-sat-mqtt 2026-11-24 (entry 477): "moreover reduce the KD coefficient from 0.05 to 0.04". SAT_PID_KD_FACTOR (SATpid.ino:28) goes from 0.05f to 0.04f: kd = 0.04 x 8400 x kp = 336 x kp (was 420 x kp). 8400 s design time and the rest of the PID unchanged. Release 2.0.0 (same theme as entries 425/427 and Robert's 2.0.0 decision for the PWM/oscillation work, not stated by Sergeant D). Comments and docs that name 0.05 or 420 x kp change with it (SATpid.ino:226 and the SAT docs).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT_PID_KD_FACTOR is 0.04f and every comment/doc/test that pins 0.05 or 420 x kp is updated (grep over src, tests, docs shows none left)
- [x] #2 Host test: the Kd of a known kp is 336 x kp with the fix and 420 x kp on origin/dev (red on origin/dev, green on the fix)
- [x] #3 evaluate.py --quick no new FAIL; full tests/ green; esp32-combo build OK
- [ ] #4 Bench check by otgw-bench-tester with the shared build (no behaviour regression of the PID; D term smaller)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 SAT_PID_KD_FACTOR 0.05 -> 0.04 (SATpid.ino:26/28/226). Every pin updated: tests/test_sat_autogains_removed.py (compiles the real _pidCalculateGains), tests/test_sat_pid_cadence.py (KD_FACTOR, comments), test_sat_pid_semantics follows cad.KD_FACTOR, tools/research/sat/sat_sim_port.py:73, docs/sat/firmware-sat-spec.md (432, 808, 873, 1028, 1142), MANUAL.md + manuals en/nl ch05/h05. Left as history: sat-comparison.md:288, ADR-197:125, the TASK-1784 study files. RED: with the tests at 0.04 and the old source test_sat_autogains_removed (1 failure), test_sat_pid_cadence (2), test_sat_pid_semantics (1) fail; GREEN after the source change, all OK. evaluate --quick: 88 passed, 0 failed. Build/bench with the shared batch build.

2026-11-25 entry 480 asked for a run on the fitted model first: OTGW-field-data/_replay_tools/q466a/sim/kdki_REPORT.txt (kdki_run.py, kdki_results.json, kdki_compare.json, kdki_room_traces.png). Fitted indigo model, 9 plant variants: Kd 0.05 -> 0.04 changes nothing on the base model in the afternoon and at night (swing 0.486 / 0.537 C, in band 49.7 / 40.2 %, starts 0.83 / 0.77 per h identical); morning ramp swing 0.117 -> 0.126 C, offset -0.181 -> -0.175 C. Per model C1 vs C0: 0 better / 9 same / 0 worse; worst corner +0.013 C swing, -1.1 percentage points in band (morning). Limits: no PWM/MM=0 loops, hot water or sun; one house. No problem found.

2026-11-25 FULL TESTS on origin/dev 686e33438 (= beta.429 build tree), after 'npm ci --prefix tests/webui': python scripts/run_tests.py --webui -j 4 --timeout 900: 475 files, 469 passed, 5 failed, 0 timeout, 1 skipped (1584.8 s). The 5 reds, none caused by this task: (1) tests/test_manual_sat_commands.py and (2) tests/test_sat_mqtt_state.py: MQTT command room_source (SATroomsource) missing in MANUAL.md and manuals; present in MQTTstuff.ino and absent from the manual already at 21e5ecb01, before beta.428 (TASK-3011/3015 lane, docs owner); (3) tests/test_sat_narrate_fits.py: SATble.ino:851 narration with an unbounded %s, from TASK-3011 (5076410fd), predates beta.428; (4) tests/test_open_questions_status.py: live queue lacks Q-466a while the table lists it; red with the older decisions pointer 71698ea too, decisions-repo bookkeeping (sat_open_questions.py --update); (5) tests/webui/settings-help.test.mjs: satroomsource help text 304 chars (limit 300), otgw-webui fixes. tests/webui/v2-undefined-names passes with acorn installed (a81749c8a). Judged against this: 'full tests green' holds except these 5 named pre-existing reds.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DRAFT 2026-11-25 (bench item pending, task NOT Done). Change: SAT_PID_KD_FACTOR 0.05 -> 0.04 (entry 477, confirmed for 2.0.0 by entry 480), kd = 0.04 x 8400 x kp = 336 x kp; SATpid.ino plus every pin: tests/test_sat_autogains_removed.py, test_sat_pid_cadence.py, test_sat_pid_semantics.py (via cad.KD_FACTOR), tools/research/sat/sat_sim_port.py, docs/sat/firmware-sat-spec.md, MANUAL.md and the EN/NL manuals. Evidence per AC: AC1 grep over src/tests/docs shows no 0.05 or 420 x kp left outside history files (sat-comparison, ADR-197, the TASK-1784 study). AC2 RED: with the tests at 0.04 on the old source test_sat_autogains_removed 1 failure, test_sat_pid_cadence 2, test_sat_pid_semantics 1; GREEN after the source change (all OK). AC3 evaluate --quick 89/0/0 on release commit 4f303da8c; affected-tests pass 422 files, 416 pass, the 5 red are pre-existing on origin/dev; esp32-combo build OK (R = 4f303da8c, 2.0.0-beta.428+4f303da, free flash 135200 B). Entry 480 model run: OTGW-field-data/_replay_tools/q466a/sim/kdki_REPORT.txt: no change on the fitted base model in the afternoon and at night; morning ramp swing 0.117 -> 0.126 C; 9 plant variants 0 better / 9 same / 0 worse. PENDING: AC4 bench (check_pid_formulas.py, D term smaller, no PID regression) by otgw-bench-tester on the soak build. Not verified: real boiler, field.
<!-- SECTION:FINAL_SUMMARY:END -->
