---
id: TASK-3041
title: >-
  SAT: reduce the Kd factor from 0.05 to 0.04 (entry 477, amends entries
  425/427)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T05:11:00Z'
updated_date: '2026-11-25T07:50:00Z'
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
- [ ] #3 evaluate.py --quick no new FAIL; full tests/ green; esp32-combo build OK
- [ ] #4 Bench check by otgw-bench-tester with the shared build (no behaviour regression of the PID; D term smaller)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 SAT_PID_KD_FACTOR 0.05 -> 0.04 (SATpid.ino:26/28/226). Every pin updated: tests/test_sat_autogains_removed.py (compiles the real _pidCalculateGains), tests/test_sat_pid_cadence.py (KD_FACTOR, comments), test_sat_pid_semantics follows cad.KD_FACTOR, tools/research/sat/sat_sim_port.py:73, docs/sat/firmware-sat-spec.md (432, 808, 873, 1028, 1142), MANUAL.md + manuals en/nl ch05/h05. Left as history: sat-comparison.md:288, ADR-197:125, the TASK-1784 study files. RED: with the tests at 0.04 and the old source test_sat_autogains_removed (1 failure), test_sat_pid_cadence (2), test_sat_pid_semantics (1) fail; GREEN after the source change, all OK. evaluate --quick: 88 passed, 0 failed. Build/bench with the shared batch build.

2026-11-25 entry 480 asked for a run on the fitted model first: OTGW-field-data/_replay_tools/q466a/sim/kdki_REPORT.txt (kdki_run.py, kdki_results.json, kdki_compare.json, kdki_room_traces.png). Fitted indigo model, 9 plant variants: Kd 0.05 -> 0.04 changes nothing on the base model in the afternoon and at night (swing 0.486 / 0.537 C, in band 49.7 / 40.2 %, starts 0.83 / 0.77 per h identical); morning ramp swing 0.117 -> 0.126 C, offset -0.181 -> -0.175 C. Per model C1 vs C0: 0 better / 9 same / 0 worse; worst corner +0.013 C swing, -1.1 percentage points in band (morning). Limits: no PWM/MM=0 loops, hot water or sun; one house. No problem found.
<!-- SECTION:NOTES:END -->
