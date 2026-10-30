---
id: TASK-1468
title: >-
  SAT: PWM exit to continuous takes effect one tick later than thermo-nova (exit
  checked after the PWM step)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T08:42:00Z'
updated_date: '2026-10-30T17:42:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 628000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1381 review (2026-10-29). thermo-nova checks the PWM exit (_maybe_disable_pwm_runtime, heating_control.py:217) before computing the control setpoint (:224-228), so the exit tick already runs the continuous rule with the previous PWM CS. The firmware calls satCycleCheckPwmExits() after satApplyPWM() (SATcontrol.ino ~5662), so the exit tick still sends a PWM CS (e.g. 10 in an OFF phase) and continuous starts one 30 s tick later. Pre-existing; changes control timing, so the fix plan needs Sergeant D's OK (quote both sides).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with thermo-nova and firmware citations posted to Sergeant D and his answer recorded in docs/sat/sergeant-d-corrections.md
- [ ] #2 If he agrees: on the exit tick the continuous rule computes the CS (host test red on HEAD, green on fix)
<!-- AC:END -->
