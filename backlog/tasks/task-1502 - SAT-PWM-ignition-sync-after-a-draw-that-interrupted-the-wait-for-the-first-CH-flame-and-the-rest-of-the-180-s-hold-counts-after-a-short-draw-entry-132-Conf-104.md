---
id: TASK-1502
title: >-
  SAT PWM: ignition sync after a draw that interrupted the wait for the first CH
  flame, and the rest of the 180 s hold counts after a short draw (entry 132,
  Conf-104)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-30T06:14:00Z'
updated_date: '2026-10-30T17:42:00Z'
labels:
  - sat sergeant-d
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 658000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-30 Conf-104: case 1 C, case 2 yes (entry 132). Rework of TASK-1422 (Done). Case 1: a draw while the ON phase still waits for its first CH flame: the ON time and the 180 s ON hold start at the first CH flame after the draw, or 180 s after the draw end without one (thermo-nova ignition sync pwm.py:186-189, :201-203); no new boiler-temperature seed, no start-up EMA (pwm.py:148-149). Today the wait before the draw counts as used ON time (SATcontrol.ino:917, :930) and the hold is skipped after the resume (:1151). Case 2: a short draw inside the first 180 s after a CH ignition: continue with the ON time left, no new measurement (103 B), and the remaining part of the original 180 s hold (draw time excluded) still counts; today the firmware drops it (:1151). Line numbers as posted in the question; re-open them at the current HEAD before implementing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test case 1: draw during the wait for the first CH flame, then flame: ON time and 180 s hold start at that flame (red on HEAD: 10 s burn in the 170 s example)
- [ ] #2 Host test case 1 fallback: no CH flame within 180 s of the draw end starts the timer anyway
- [ ] #3 Host test case 2: short draw 60 s after ignition: the remaining 120 s of hold still applies after the draw; no new temperature seed
- [ ] #4 build + evaluate green; firmware-sat-spec.md updated
<!-- AC:END -->
