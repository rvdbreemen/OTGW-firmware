---
id: TASK-8
title: 'PID integral: only active outside deadband (SAT Python conformity)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:05'
updated_date: '2026-04-05 11:16'
labels:
  - sat
  - bugfix
  - pid
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Critical deviation from SAT Python: the current port activates the integral INSIDE the deadband and resets outside. SAT Python does the OPPOSITE: integral is only active when |error| > DEADBAND (0.1 in Python). Inside the deadband, integral is reset to 0. The rationale: the integral is meant to correct steady-state offset when the proportional term alone is not enough. Inside the deadband the target is reached and the integral should not accumulate further. Also: SAT Python clamps integral to [0, heating_curve_value] (positive only), not [-curveValue, +curveValue]. This prevents the integral from going negative and pulling the setpoint too far down.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Integral active when |error| > deadband (was: <= deadband)
- [x] #2 Integral reset to 0.0 when |error| <= deadband (was: active)
- [x] #3 Integral clamp range changed to [0, curveValue] (was: [-curveValue, +curveValue])
- [x] #4 Hard cap remains at 20.0 (not -20.0 to +20.0, only 0 to 20.0)
- [x] #5 Integral time limit removed or adjusted (SAT Python has no 300s delay)
- [x] #6 Test scenario documented: error outside deadband -> integral accumulates; error inside deadband -> integral = 0
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SATpid.ino to understand current integral logic
2. Fix _pidUpdateIntegral: swap deadband condition (active outside, reset inside)
3. Change integral clamp range from [-curveValue, +curveValue] to [0, curveValue]
4. Change hard cap from [-20, +20] to [0, 20]
5. Remove or adjust the 300s integral time limit
6. Verify derivative also resets inside deadband (cross-reference with task 7)
7. Test: verify PID output is correct with the new logic
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fixed _pidUpdateIntegral: integral now active outside deadband (was inside). Clamp range changed to [0, curveValue]. Hard cap now [0, 20]. Removed 300s time limit and timer tracking - uses fixed 60s interval per SAT Python. Simplified function significantly.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed critical PID integral logic inversion per SAT Python (pid.py).

Changes:
- Integral now accumulates when |error| > deadband (was incorrectly active inside deadband)
- Integral resets to 0 when |error| <= deadband (was incorrectly resetting outside)
- Clamp range changed from [-curveValue, +curveValue] to [0, curveValue] (positive only)
- Hard cap changed from [-20, +20] to [0, 20]
- Removed 300s integral time limit (SAT Python has none)
- Uses fixed 60s interval (SAT_PID_UPDATE_INTERVAL) instead of elapsed time tracking
- Removed unused _pid_lastIntegralMs variable and SAT_PID_INTEGRAL_TIME_LIMIT constant

Files modified: SATpid.ino
<!-- SECTION:FINAL_SUMMARY:END -->
