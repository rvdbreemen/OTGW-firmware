---
id: TASK-7
title: 'PID derivative filter improvement: adaptive alpha'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:04'
updated_date: '2026-04-05 23:12'
labels:
  - sat
  - bugfix
  - pid
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python uses an adaptive alpha for the derivative low-pass filter: alpha = delta_time / (PID_UPDATE_INTERVAL + delta_time). This automatically adapts to the sample rate. The current port uses a fixed alpha of 0.8. Since we're closer to the OT bus on the ESP and can potentially sample faster (control interval configurable 10-300s), the adaptive alpha is essential for correct derivative calculation at different intervals. Also: SAT Python uses temperature-based derivative with NEGATIVE sign (rising temp = negative derivative = damping), which is correct for heating control. Verify the current implementation does this correctly. SAT Python resets derivative to 0 when error falls within deadband.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Derivative alpha calculation changed to adaptive: alpha = dt / (PID_UPDATE_INTERVAL + dt)
- [x] #2 Derivative calculation validates correct negative sign convention
- [x] #3 Derivative skip when delta_time < PID_UPDATE_INTERVAL (prevents noise at fast sampling)
- [x] #4 REST API: GET /api/v2/sat/status includes raw_derivative field
- [x] #5 MQTT publish: sat/raw_derivative
- [x] #6 WebUI: raw derivative value visible in PID Details section
- [x] #7 Derivative FREEZES at last calculated value when error falls within deadband (not reset to 0)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Fix _pidUpdateDerivative: FREEZE derivative (keep last value) when error inside deadband, instead of resetting to 0\n2. Update task #7 AC #7\n3. Fix task #8 integral logic simultaneously: swap deadband conditions\n4. Integral active INSIDE deadband, reset OUTSIDE\n5. Clear old final summaries, add corrected ones\n6. Commit with descriptive title
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
sergeantd feedback (2026-04-05): Derivative should FREEZE (not reset to 0) inside deadband. Last derivative value persists as heating curve offset. SAT design: outside deadband = P + Heating Curve + active Derivative; inside deadband = P + Heating Curve + frozen Derivative + active Integral.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed PID derivative deadband behavior per sergeantd's SAT design feedback.\n\nThe derivative now FREEZES at its last calculated value when error falls within the deadband, instead of resetting to 0. This matches the SAT Python design where frozen derivative persists as a heating curve offset inside the deadband.\n\nSAT PID zones:\n- Outside deadband: P + HeatingCurve + active Derivative\n- Inside deadband: P + HeatingCurve + frozen Derivative + active Integral\n\nFiles modified: SATpid.ino
<!-- SECTION:FINAL_SUMMARY:END -->
