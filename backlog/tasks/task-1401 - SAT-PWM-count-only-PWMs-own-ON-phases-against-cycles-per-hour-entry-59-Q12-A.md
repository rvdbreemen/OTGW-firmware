---
id: TASK-1401
title: >-
  SAT PWM: count only PWM's own ON phases against cycles per hour (entry 59, Q12
  A)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T20:29:00Z'
updated_date: '2026-10-26T19:16:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 540000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 19:52 (Q12: A, entry 59; context entry 58). thermo-nova counts only its own PWM ON-phase starts (pwm.py:159-160) in a rolling hour (:138-141); hot-water and relight flame starts never count. Firmware counts every flame-on edge in the rolling hour and blocks a new PWM ON phase at cycles per hour (SATcycles.ino:279-299), which caused gruijter's 56-minute heating gap after a shower (Q12 evidence). Change: the hourly counter counts PWM ON-phase starts only; limit = cycles per hour. Note thermo-nova's own check compares with duty_cycle_seconds (pwm.py:156) and so never blocks; entry 59 asks for the cycles-per-hour limit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Hot-water and relight flame starts do not count; only PWM ON-phase starts count against cycles per hour (host test red on HEAD with the shower scenario)
- [ ] #2 A 4th PWM ON start in the rolling hour at 3 cycles/h is still blocked
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-26 Robert: implement now with priority, together with TASK-1369 (PWM-entry ON counts, never blocked by the limit; see TASK-1369 notes).
<!-- SECTION:NOTES:END -->
