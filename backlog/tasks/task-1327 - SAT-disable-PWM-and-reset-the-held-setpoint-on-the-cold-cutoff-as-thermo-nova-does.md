---
id: TASK-1327
title: >-
  SAT: disable PWM and reset the held setpoint on the cold cutoff, as
  thermo-nova does
status: To Do
assignee: []
created_date: '2026-10-23T17:53:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 450000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1323 review (opus), pre-existing, now hit more often because the solar offset can keep the requested setpoint at or below the cold setpoint for hours: thermo-nova calls self._pwm.disable() and sets _control_setpoint = MINIMUM_SETPOINT when the heater state is OFF (heating_control.py:229-234; pwm.py:106-109 disable resets PWM). Our firmware stays in SAT_MODE_PWM, satPwmSchedule keeps its phase timers running on cold ticks, and _sat_lastControlCS keeps the pre-cutoff value (SATcontrol.ino ~5003 vs the cutoff ~5030). Scenario: forced PWM, overshoot protection off, solar keeps requested at 27 (CS=10, CH=0); when solar clears to 30, satApplyPWM returns the stale prevCS (e.g. 45) with CH=1, where thermo-nova would start from a reset PWM. Needs Sergeant D's OK (SAT control).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's choice recorded (mirror pwm.disable() and/or reset the held CS on the cutoff), with thermo-nova citations
- [ ] #2 Host test: cutoff then recovery no longer returns the stale held CS (red before, green after)
<!-- AC:END -->
