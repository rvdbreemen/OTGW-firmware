---
id: TASK-1382
title: >-
  SAT PWM: time the ON phase from its start and keep waiting for ignition as
  thermo-nova (Q35)
status: To Do
assignee: []
created_date: '2026-10-25T10:38:00Z'
updated_date: '2026-10-28T06:48:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 521000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 / TASK-1366 R3. Firmware times ON expiry from the last flame-on edge and aborts the ON phase after a 180 s ignition timeout; thermo-nova times from the phase start, resyncs to the first ignition and continues (pwm.py:181-215, :197-199). Blocked on Q35. Scope together with TASK-1380 (OFF anchor).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ON phase timed from the phase start/first-ignition sync, no flip on a slow ignition (r3 host scenario green; expected-failure marker in tests/test_sat_hp_pwm.py removed)
- [ ] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q35 A decided (entry 80). Field reproduction (entry 92): gruijter alpha.514 capture 2026-10-27, an ON phase that starts with the flame off anchors on the last hot-water ignition (SATcontrol.ino:883/:1024) and ends on its first tick: CS=48 10:14:24, CS=10 10:14:54, CS=48 10:15:24 CET, flame 10:15:35. Blocks TASK-1419 (Q45) and TASK-1421 (Q49 B).
<!-- SECTION:NOTES:END -->
