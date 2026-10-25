---
id: TASK-1382
title: >-
  SAT PWM: time the ON phase from its start and keep waiting for ignition as
  thermo-nova (Q35)
status: To Do
assignee: []
created_date: '2026-10-25T10:38:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
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
