---
id: TASK-1787
title: >-
  SAT: Sergeant D concern, coefficient self-tune moves too fast (gruijter at
  1.12)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T18:57:00Z'
updated_date: '2026-11-10T22:24:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 880000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #dev-sat-mqtt 2026-11-10 19:48 UTC, msg 1569785822394323056 (relayed by the coordinator): the coefficient autotune seems to move very fast; 1.12 is very low; the curve value is the integral-like base, so a low coefficient acts like an overdamped integral. About gruijter's screenshot of 2026-11-07 (#alpha-testing msg 1568595554416595025). A concern, not a decision: no corrections entry yet. Findings: thermo-nova at 7beaaf5 has no coefficient autotune (coefficient only read from options: entry_data.py:213, heating_curve.py:15/25/39, config_flow.py:616/804; checked by a second reader). Our tuner (ADR-205, SATcurveTune.h:8-13, SATcontrol.ino:6054-6090): at most one 0.1 step per 3 counted days, window emptied after a change. First shipped in v2.0.0-beta.1 (2026-11-01), so by 2026-11-07 it can have moved at most 0.2 (two decisions), and 1.12 is not on the 0.1 grid from the default 2.0: the tuner alone cannot have taken 2.0 to 1.12. Local data: capture-gruijter-1109 has no coefficient column; no SATTUNE/SATTUNED log of his is local.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Origin of gruijter's 1.12 established from his SATTUNE/SATTUNED/SATTUNER log lines or settings history
- [x] #2 Question to Sergeant D posted via 97 with options and a recommendation; his answer recorded as a numbered entry
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 030509: question posted by 97 in #dev-sat-mqtt as a reply to his msg, msg 1569788551833329848; options A (ask gruijter for his SATTUNE/SATTUNED/SATTUNER log lines, recommended), B (slow the tuner), C (keep the tuner inside 1.6-2.4, entry 166). Correction by 97's second reader: the tuner landed on dev on 2026-10-31 (0fd940bf7, TASK-1525; ADR-205 ce5e9e6bb), so alpha testers had it from 31 Oct, not from beta.1 on 1 Nov; 31 Oct to 7 Nov still allows at most 2 decisions (0.2), so the conclusion stands. AC2 is half done: the answer is still to be recorded as an entry; AC2 stays open until the entry exists.

2026-11-10 030509: Sergeant D answered 19:06:15 UTC (msg 1569790312220590140, time from the message id): '@BlueFlame Bot let’s wait guitars night log and decide later' ('guitars' = gruijter's, our interpretation). Recorded as entry 298: option A, no tuner change until gruijter's night log is in. 97 acked (1569790712331903141) and asked gruijter for the log (#alpha-testing 1569790714794221639). Time note: the description says 19:48 UTC for his concern; the message id gives 18:48 UTC. Next: read gruijter's SATTUNEH/SATTUNED/SATTUNER/SATTUNE lines when they arrive (AC1), then bring the result back to him.

Code check by otgw-firmware-a4 (2026-11-10, origin/dev 4595fa65a): the ADR-205 tuner cannot have produced 1.12. It decides at most once per 3 counted days (window emptied after every change, SATcontrol.ino:6064-6084) and moves exactly 0.1, rounded to 0.01 (:6080-6082); from the default 2.0 (SATtypes.h:500) it only reaches values ending in 0. Since the tuner landed (0fd940bf7, 2026-10-31) at most about 3 steps fit. Other writers: settings POST (settingStuff.ino:1105-1109), REST heating_curve_coeff (restAPI.ino:1671), MQTT command heating_curve_coeff (MQTTstuff.ino:717), the onboarding wizard (v2.js:7905, range step 0.1, so x.x only) and the settings field (free number).

Likely origin (inference, not proven): the old firmware-only hourly auto-tune removed by TASK-1413 (02029bf75, 2026-10-30, alpha.562). It multiplied the coefficient every hour by (1-2r), (1-r) or (1+r) with r = 2 % (old SATtypes.h:550), so it gives arbitrary decimals; 2.0 to 1.12 is about 29 steps of -2 % or 14 of -4 %. It was off by default (bAutoTune = false), switched on through satautotune or the HA switch 'SAT Auto Tune' whose code comment read 'automatic PID gains tuning'. After its removal a stored coefficient loads unchanged (commit message of 02029bf75). To confirm with gruijter: did he run a 2.0 alpha before 2026-10-30 with SAT Auto Tune on, and does an HA automation or retained MQTT message write heating_curve_coeff?

Capture 20261110-100556 (gruijter, beta.167+e79e495, 10:05-21:15 local): settings dump curve_coeff 1.12, curve_self_tune true; 11 SATTUNEH hours (hour 0-10, tuner restarted ~09:59), none counted (not_sat 1, sun 5, pwm 5); no SATTUNED, SATTUNE or SATTUNER line, so the tuner changed nothing in this run. Hour means +1.97 to +20.70 C, far above the 1.0 band: counted hours would have pushed a raise, not a lower. The zip also holds coredump-20261110-100556.bin (24.9 KB), not decoded yet. Sergeant D asked gruijter at 20:34 UTC to raise the coefficient to 1.8 (entry 301); that manual change empties the window, so tonight's log will not show the path to 1.12.
<!-- SECTION:NOTES:END -->
