---
id: TASK-1383
title: >-
  SAT PWM: start an ON phase only when the mapped ON time is at least 180 s
  (Q38)
status: To Do
assignee: []
created_date: '2026-10-25T10:39:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 522000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273. thermo-nova starts ON only when on_time_seconds >= HEATER_STARTUP_TIMEFRAME (pwm.py:155, const.py:18); the firmware OFF branch has no such gate, so the ultra-low no-flame case requests ON after 2P. Blocked on Q38.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No ON start when the mapped ON time is below 180 s (host test red on HEAD, green on fix)
- [ ] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->
