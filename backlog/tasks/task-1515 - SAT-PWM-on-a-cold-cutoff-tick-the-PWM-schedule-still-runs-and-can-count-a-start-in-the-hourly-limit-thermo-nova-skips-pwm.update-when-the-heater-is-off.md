---
id: TASK-1515
title: >-
  SAT PWM: on a cold-cutoff tick the PWM schedule still runs and can count a
  start in the hourly limit (thermo-nova skips pwm.update when the heater is
  off)
status: To Do
assignee: []
created_date: '2026-10-30T18:16:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 671000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1468 review (2026-10-30). In satControlLoopRun the PWM schedule (satPwmSchedule) runs before satPwmDisableOnHeaterOff, so a tick where the requested setpoint drops to the cold setpoint (heater OFF) can still advance the PWM state machine and count an ON start against the hourly limit. thermo-nova does not call pwm.update when the heater state is OFF (heating_control.py:202-215 under other-projects/SAT-releases-thermo-nova/custom_components/sat; verify the exact lines before citing). Pre-existing, not introduced by TASK-1468. Check against Sergeant D entries on the cold cutoff (ADR-199, entries 7, 11, 12, 98) before changing behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 thermo-nova behaviour on a heater-OFF tick cited (file:line, second reader)
- [ ] #2 Host test: cold-cutoff tick in PWM does not count a start (red on HEAD, green on fix), after Sergeant D's OK if it changes behaviour
<!-- AC:END -->
