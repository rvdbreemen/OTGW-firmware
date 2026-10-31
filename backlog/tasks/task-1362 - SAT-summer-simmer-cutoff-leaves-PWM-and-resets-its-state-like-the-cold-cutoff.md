---
id: TASK-1362
title: 'SAT: summer simmer cutoff leaves PWM and resets its state like the cold cutoff'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T20:26:00Z'
updated_date: '2026-10-31T05:28:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 497000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-199 open question 2), audit #49: the firmware-only summer simmer path (SATcontrol.ino ~4930-4942) sends CS=10/CH=0 but does not call satPwmDisableOnHeaterOff() as the cold cutoff and valves-closed paths do, so one odd PWM period follows when summer mode ends. Call it there too (PWM off, state reset, MM back to max).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a summer simmer cutoff PWM is off and its state reset (host test red on HEAD, green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (sonnet lane wt-1362): summer simmer block calls satPwmDisableOnHeaterOff() after fFinalSetpoint = SAT_MIN_SETPOINT (same helper as the cold cutoff and valves-closed paths); _sat_lastControlCS line kept. tests/test_sat_pwm_override.py: base Ran 79 FAILED (failures=3: PWM still requested/hold 48 after summer; recovery CS 40 stale vs 62 fresh; source check), fix 79 OK. Interactions checked: ADR-203 cache still CS=10/MM max/CH 0; flood and 3 s refresh need PWM; TASK-1502 DHW pause state cleared by satPwmReset. Self-review only (one helper call, same as cold cutoff); full suite at integration.

2026-10-31 (session otgw-firmware-ea): lane wt-v1362 holds the change (satPwmDisableOnHeaterOff() in the summer simmer branch of satControlLoopRun + test). Not merged yet: summer simmer has no thermo-nova counterpart, and entry 12 settles only the cold cutoff, so extending the heater-OFF rule is our reading of Robert's ADR-199 OQ2 decision and changes how SAT drives the boiler; CLAUDE.md SAT rules require Sergeant D's OK first. Queued as Q-1362a (after Q-1458a).
<!-- SECTION:NOTES:END -->
