---
id: TASK-7
title: 'PID derivative filter improvement: adaptive alpha'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:04'
updated_date: '2026-04-05 11:33'
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
- [x] #3 Derivative reset to 0 when error falls within deadband (per SAT Python)
- [x] #4 Derivative skip when delta_time < PID_UPDATE_INTERVAL (prevents noise at fast sampling)
- [x] #5 REST API: GET /api/v2/sat/status includes raw_derivative field
- [x] #6 MQTT publish: sat/raw_derivative
- [x] #7 WebUI: raw derivative value visible in PID Details section
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read current derivative implementation in SATpid.ino
2. Change fixed alpha (0.8) to adaptive: alpha = dt / (PID_UPDATE_INTERVAL + dt)
3. Verify negative sign convention (rising temp = negative derivative)
4. Add derivative reset to 0 when error inside deadband
5. Add skip when delta_time < PID_UPDATE_INTERVAL
6. Add raw_derivative to REST status JSON
7. Add MQTT publish sat/raw_derivative
8. Add WebUI display in PID Details section
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed PID derivative filter to match SAT Python (pid.py) implementation.

Changes:
- Adaptive alpha: alpha = dt / (PID_UPDATE_INTERVAL + dt) instead of fixed 0.8
- Added negative sign convention: rising temp produces negative derivative (damping)
- Derivative resets to 0 when error falls within deadband
- Skip when delta_time <= PID_UPDATE_INTERVAL (prevents noise at fast sampling)
- Hard cap applied before filter on extreme values (SAT Python order)
- Added state.sat.fRawDerivative for diagnostics
- REST API: raw_derivative field in GET /api/v2/sat/status
- MQTT: sat/raw_derivative published
- WebUI: Raw Derivative row in PID Details section

Files modified: SATpid.ino, OTGW-firmware.h, SATcontrol.ino, index.html, sat.js
<!-- SECTION:FINAL_SUMMARY:END -->
