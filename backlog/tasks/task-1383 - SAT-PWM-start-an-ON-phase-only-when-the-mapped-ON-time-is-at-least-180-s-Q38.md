---
id: TASK-1383
title: >-
  SAT PWM: start an ON phase only when the mapped ON time is at least 180 s
  (Q38)
status: To Do
assignee: []
created_date: '2026-10-25T10:39:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
