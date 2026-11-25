---
id: TASK-3041
title: >-
  SAT: reduce the Kd factor from 0.05 to 0.04 (entry 477, amends entries
  425/427)
status: To Do
assignee: []
created_date: '2026-11-25T05:11:00Z'
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
- [ ] #1 SAT_PID_KD_FACTOR is 0.04f and every comment/doc/test that pins 0.05 or 420 x kp is updated (grep over src, tests, docs shows none left)
- [ ] #2 Host test: the Kd of a known kp is 336 x kp with the fix and 420 x kp on origin/dev (red on origin/dev, green on the fix)
- [ ] #3 evaluate.py --quick no new FAIL; full tests/ green; esp32-combo build OK
- [ ] #4 Bench check by otgw-bench-tester with the shared build (no behaviour regression of the PID; D term smaller)
<!-- AC:END -->
