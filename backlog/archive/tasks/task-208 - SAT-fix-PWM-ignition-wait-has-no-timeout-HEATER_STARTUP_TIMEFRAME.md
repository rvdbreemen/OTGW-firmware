---
id: TASK-208
title: 'SAT fix: PWM ignition wait has no timeout (HEATER_STARTUP_TIMEFRAME)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:24'
updated_date: '2026-04-09 06:15'
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
- [x] #1 Add 180s ignition timeout to _pwm_waitingForFlame logic in satApplyPWM()
- [x] #2 Log warning when timeout fires without flame detection
- [x] #3 Start PWM timer normally after timeout (matching Python behavior)
- [x] #4 Verify no regression in normal ignition path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add static _pwm_waitForFlameStartMs to track when waitingForFlame begins
2. In the _pwm_waitingForFlame=true set-point (line 574), record timestamp
3. In step 2 of PWM state machine (line 631-637), check if elapsed > 180s
4. On timeout: clear _pwm_waitingForFlame, clear bPwmFlameRequested, log warning, return SAT_MIN_SETPOINT (OFF)
5. On flame detection: clear the timeout timestamp (normal path unchanged)
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 180s ignition timeout to the _pwm_waitingForFlame branch in satApplyPWM(). New static _pwm_waitForFlameStartMs tracks when the wait begins (set at both entry points where _pwm_waitingForFlame=true). If PWM_IGNITION_TIMEOUT_MS (180s) elapses without flame detection, logs a warning, aborts the ON phase (bPwmFlameRequested=false), and returns SAT_MIN_SETPOINT. Normal ignition path (flame detected) clears the timeout timestamp immediately. Matches Python pwm.py:194-202 HEATER_STARTUP_TIMEFRAME behavior.
<!-- SECTION:FINAL_SUMMARY:END -->
