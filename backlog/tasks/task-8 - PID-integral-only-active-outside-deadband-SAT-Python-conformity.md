---
id: TASK-8
title: 'PID integral: only active outside deadband (SAT Python conformity)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:05'
updated_date: '2026-04-05 23:12'
labels:
  - sat
  - bugfix
  - pid
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT custom PID design: integral is ONLY active INSIDE the deadband as a smooth compensator for external heat sources (sun, cooking, activity). Outside the deadband, the heating curve replaces the integral role. This is a deliberate departure from traditional PID where integral corrects steady-state offset. In SAT, the heating curve handles that job outside the deadband. Clamp range [0, curveValue] (positive only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Integral clamp range changed to [0, curveValue] (was: [-curveValue, +curveValue])
- [x] #2 Hard cap remains at 20.0 (not -20.0 to +20.0, only 0 to 20.0)
- [x] #3 Integral time limit removed or adjusted (SAT Python has no 300s delay)
- [x] #4 Integral active when |error| <= deadband (compensator for external heat sources)
- [x] #5 Integral reset to 0.0 when |error| > deadband (heating curve takes over)
- [x] #6 Test scenario: error inside deadband -> integral accumulates smoothly; error outside deadband -> integral = 0
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

sergeantd feedback (2026-04-05): Task needs REVERSAL. Integral should be active INSIDE deadband (as compensator for sun/cooking/activity), NOT outside. Outside deadband the heating curve replaces the integral role. SAT design: outside = P + Heating Curve + active Derivative (no integral); inside = P + Heating Curve + frozen Derivative + active Integral. Ref: https://github.com/Alexwijn/SAT/blob/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb/custom_components/sat/pid.py#L185
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed critical PID integral deadband logic per sergeantd's SAT design feedback.\n\nThe integral now activates INSIDE the deadband (as compensator for external heat sources like sun/cooking) and resets to 0 OUTSIDE the deadband (where the heating curve takes over). This was previously inverted.\n\nSAT PID zones:\n- Outside deadband: P + HeatingCurve + active Derivative (integral = 0)\n- Inside deadband: P + HeatingCurve + frozen Derivative + active Integral\n\nClamp: [0, curveValue], hard cap 20.0. No time limit (SAT Python has none).\n\nFiles modified: SATpid.ino
<!-- SECTION:FINAL_SUMMARY:END -->
