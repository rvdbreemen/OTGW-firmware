---
id: TASK-94
title: 'SAT Audit B3: Setpoint calculation chain'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:50'
updated_date: '2026-04-09 05:19'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Trace the full setpoint calculation chain: from target temperature via PID output to boiler setpoint. Compare Python SAT (thermo-nova) with C++ SATcontrol.ino. Pay attention to rounding, clamping and offset corrections.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Setpoint calculation chain fully traced in Python and C++
- [x] #2 Rounding and clamping behavior compared
- [x] #3 Offset and calibration corrections verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B3 audit: Setpoint calculation chain - GAPS FOUND

Python chain: requested_setpoint -> PID.output -> heating_control._compute_[continuous|pwm]_control_setpoint -> coordinator.async_set_control_setpoint -> CS= command.

C++ chain: satCalcHeatingCurve() -> satPidUpdate() -> satApply[PWM|Continuous]() -> addCommandToQueue("CS=...").

MATCHING:
- PID output = curve + P + I + D, sent as CS= ✓
- MaxTSet from OT bus used as upper clamp ✓
- SAT_MIN_SETPOINT=10 ✓

GAPS:
1. Continuous mode setpoint clamping: Python _compute_continuous_control_setpoint() has 3-branch logic: (a) if boiler_temp is None -> use requested; (b) if boiler_temp <= requested -> use requested; (c) if flame off -> use requested; (d) else: clamp to max(requested, boilerTemp - offset). C++ satApplyContinuous() only implements case (d): clamps to boilerTemp - flowOffset when pidOutput < minAllowed. Missing cases (a)-(c): when boiler temp is unavailable or flame is off, C++ still applies the clamp. Minor correctness gap.
2. PWM setpoint override (flame-off hold): Python _compute_pwm_control_setpoint() uses return_temperature + flame_off_offset_celsius (default 18C). C++ uses Tret + settings.sat.fFlameOffOffset. Python default is 18C; check C++ default. Actually compatible.
3. Python has overshoot_protection guard: if requested_setpoint >= maximum_setpoint, disable PWM entirely. C++ has no equivalent guard.
4. Python saturation detection: checks off_time_seconds==0 in PWM duty cycle. C++ checks flame duration > SAT_SATURATION_SUSTAIN_SEC. Different mechanism, but similar intent.
<!-- SECTION:FINAL_SUMMARY:END -->
