---
id: TASK-1518
title: >-
  SAT: no CH flame after a hot-water draw lets the PWM minimum-burn hold lapse
  (Sergeant D entry 158, Q-1502a B)
status: To Do
assignee: []
created_date: '2026-10-31T05:12:00Z'
labels:
  - sat
  - pwm
dependencies: []
priority: medium
ordinal: 674000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entry 158 (Q-1502a B): a PWM ON phase that started with the flame burning, is interrupted by a draw and gets no CH flame after it ends on its normal ON time; the 180 s hold does not apply without a flame. Today (alpha.564, TASK-1502) the firmware freezes the hold clock through the draw and the 180 s post-draw wait and then applies the rest of the hold (ON phase ends at 390 s in the posted example instead of 270 s). thermo-nova measures the hold from flame-on by the clock (pwm.py:144-146, :200-202). Probe P4 of the TASK-1502 review reproduces it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real SATcontrol.ino: burning start, draw, no CH flame after it: the ON phase ends on its ON time (red on alpha.564, green on fix)
- [ ] #2 TASK-1502 cases 1 and 2 (tests/test_sat_pwm_anchors.py) stay green
- [ ] #3 build + evaluate green; firmware-sat-spec.md updated
<!-- AC:END -->
