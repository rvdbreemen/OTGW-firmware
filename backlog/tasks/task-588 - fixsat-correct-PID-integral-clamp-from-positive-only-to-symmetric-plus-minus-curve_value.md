---
id: TASK-588
title: >-
  fix(sat): correct PID integral clamp from positive-only to symmetric
  plus-minus curve_value
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-08 16:48'
updated_date: '2026-05-08 17:36'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The C++ SATpid.ino clamps the integral to [0, curveValue] (positive only, plus a hard cap at 20 degrees). The Python reference uses clamp_to_range(integral, curve_value) which allows [-curve_value, +curve_value]. In mild weather when the room temperature overshoots the setpoint slightly, a negative integral accumulation is needed to nudge the flow setpoint below the heating curve. The positive-only clamp silently prevents this correction, causing SAT to under-react to mild overshoot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Integral clamp in SATpid.ino changed from [0, curveValue] to [-curveValue, +curveValue]
- [ ] #2 Positive and negative integral accumulation both work within the band
- [ ] #3 Hard overflow cap preserved symmetrically at +/-20 degrees (or +-curveValue if curveValue > 20)
- [ ] #4 Existing integration tests pass; no regression in normal heating scenarios
- [ ] #5 Change documented in code comment citing Python reference
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SATpid.ino to locate integral clamp
2. Change clamp from [0, curveValue] to [-curveValue, +curveValue]
3. Add negative hard cap to match positive SAT_PID_INTEGRAL_ABS_MAX
4. Update function comment (was 'positive only')
5. Build firmware to verify clean compile
6. Commit and push
<!-- SECTION:PLAN:END -->
