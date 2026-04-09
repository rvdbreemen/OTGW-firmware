---
id: TASK-208
title: 'SAT fix: PWM ignition wait has no timeout (HEATER_STARTUP_TIMEFRAME)'
status: To Do
assignee: []
created_date: '2026-04-09 05:24'
labels:
  - audit-fix
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python pwm.py:194-202 implements a 180s (HEATER_STARTUP_TIMEFRAME) timeout for flame detection in the PWM ON phase: if the boiler does not ignite within 180 seconds, it starts the timer anyway and logs a warning. C++ _pwm_waitingForFlame flag never times out - if the boiler fails to ignite, C++ stays in the 'waiting for flame' branch indefinitely, sending a low return-water-based CS setpoint forever. In normal operation this may not matter (boiler will fault), but for fault recovery and cycle tracking it is a behavioral difference.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Add 180s ignition timeout to _pwm_waitingForFlame logic in satApplyPWM()
- [ ] #2 Log warning when timeout fires without flame detection
- [ ] #3 Start PWM timer normally after timeout (matching Python behavior)
- [ ] #4 Verify no regression in normal ignition path
<!-- AC:END -->
