---
id: TASK-1369
title: 'SAT PWM: the first phase after PWM starts is ON (entry 43)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T21:43:00Z'
updated_date: '2026-10-26T20:39:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 504000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q11, entry 43): first PWM phase is ON, as thermo-nova pwm.py (audit #17: firmware starts OFF timed from the last flame-off).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 When PWM starts the first phase is ON at once (host test red/green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-26 Robert (maintainer decision, provisional until Sergeant D answers Q48): the ON phase that starts when PWM is entered with the burner already running (entries 22, 43) COUNTS as a PWM ON start for the hourly limit (entry 59, TASK-1401) but the limit never blocks or cuts that entry start. Rationale: entry 43 literally; thermo-nova counts it (pwm.py:160) and its cap check (pwm.py:156) compares with duty_cycle_seconds, so in practice it never blocks. Removes gruijter's 12:31 cut (2026-10-26 capture).

2026-10-26 implemented together with TASK-1401 (same PWM start gate): the PWM-entry ON phase (burner already burning) starts at once when the on time is >= 180 s (pwm.py:154-155, const.py:18), counts as a PWM start, and is never cut by the hourly limit (maintainer decision, provisional Q48). Tests in tests/test_sat_pwm_start_limit.py (entry, entry5, entryflow, entrylimit, lag, wrap cases).
<!-- SECTION:NOTES:END -->
