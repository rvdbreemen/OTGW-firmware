---
id: TASK-1383
title: >-
  SAT PWM: start an ON phase only when the mapped ON time is at least 180 s
  (Q38)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T10:39:00Z'
updated_date: '2026-10-28T09:25:00Z'
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
- [x] #1 No ON start when the mapped ON time is below 180 s (host test red on HEAD, green on fix)
- [x] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q38 A decided (entry 80): start an ON phase only when the mapped ON time is >= 180 s. After or with TASK-1382.

2026-10-28: bundled into the TASK-1382/1380 candidate (advisor review: without the Q38 gate the new ignition wait would fire the burner in range 1 with no flame every 2P). Q38 A recorded as entry 80.

AC#2: Q38 A recorded as entry 80 before implementation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OFF->ON only when the mapped ON time >= 180 s (pwm.py:155, Q38 A entry 80), so duty range 1 without flame never starts ON. Commit de23e3ea1. AC#1: D5 in tests/test_sat_pwm_anchors.py red on the pre-fix source (ON starts every 60 s from t=1020), green on the fix. AC#2: entry 80. Note (review): in ranges 2-4 ON is clamped to >= 180 s, so the gate bites only in range 1.
<!-- SECTION:FINAL_SUMMARY:END -->
