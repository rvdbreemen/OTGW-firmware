---
id: TASK-1558
title: >-
  SAT PWM: a CH flame after a hot-water draw gets the 180 s minimum burn hold
  also when the first-flame wait had timed out (Q-1502b A, entry 211)
status: To Do
assignee: []
created_date: '2026-11-02T05:45:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 710000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-02 answered Q-1502b with A (entry 211): an ON phase whose 180 s first-flame wait had already timed out, then a hot-water draw, then a CH flame: that flame gets the 180 s minimum burn hold, as without a draw. thermo-nova pwm.py:144-146 / :201-203 (hold starts at the first flame seen after a timed-out wait). Today the phase can end sooner (330 s example in TASK-1502). Related: TASK-1518 (entry 158).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code: timed-out wait, draw, CH flame -> the ON phase holds 180 s from that flame (red on HEAD, green)
- [ ] #2 No change when the draw comes before the wait times out (TASK-1502 behaviour kept)
<!-- AC:END -->
