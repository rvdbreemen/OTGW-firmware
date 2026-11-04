---
id: TASK-1515
title: >-
  SAT PWM: on a cold-cutoff tick the PWM schedule still runs and can count a
  start in the hourly limit (thermo-nova skips pwm.update when the heater is
  off)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-30T18:16:00Z'
updated_date: '2026-11-04T06:39:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: satPwmSchedule (SATcontrol.ino:6734) runs before the cold-cutoff disable (:6796). AC1 + red test doable; behaviour change needs Sergeant D OK.

2026-11-04 Robert decided (maintainer decision, entries 257-262; Sergeant D informed msg 1567417361798144021, may object). See docs/sat/sergeant-d-corrections.md.

2026-11-04 implemented (sonnet agent; lead reran on the integration tree): see CHANGELOG [Unreleased] and docs/sat/firmware-sat-spec.md. Host tests red on old, green on fix: test_sat_pwm_start_limit 26 OK (1515), test_pic_pm_chain 40 OK (1384), test_sat_no_return_startup 20 OK (1412.12), overshoot_sustain 62 OK, no_return_ignition 19 OK, evaluate 0 failed. Shipped together as beta.66 (one SATcontrol.ino commit for TASK-1515, TASK-1384, TASK-1412.12). Bench pending.
<!-- SECTION:NOTES:END -->
