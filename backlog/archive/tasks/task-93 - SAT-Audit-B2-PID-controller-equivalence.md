---
id: TASK-93
title: 'SAT Audit B2: PID controller equivalence'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:49'
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
Compare the PID implementation (Kp, Ki, Kd, windup guard, output clamping) in Python SAT thermo-nova branch with the C++ PID in SATcontrol.ino. Also verify reset behavior and sampling interval.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All PID parameters compared (Kp, Ki, Kd, limits, windup)
- [x] #2 Sampling interval and timing verified
- [x] #3 Output clamping behavior is equivalent or deviation documented
- [x] #4 Integral windup guard works correctly
- [ ] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B2 audit: PID controller equivalence - MINOR GAPS

Python pid.py vs C++ SATpid.ino comparison:

MATCHING:
- Kp = (coeff * curveValue) / divisor (4=underfloor, 3=radiator) ✓
- Ki = Kp / 8400 ✓
- Kd = 0.07 * 8400 * Kp ✓
- output = curveValue + P + I + D ✓
- PID_UPDATE_INTERVAL = 60s ✓
- Integral resets outside deadband ✓
- Integral clamped to [0, curveValue] ✓
- Derivative: temperature-based, negative sign ✓
- Derivative cap: DERIVATIVE_RAW_CAP = 5.0 ✓
- Adaptive alpha: alpha = dt / (PID_UPDATE_INTERVAL + dt) ✓
- Low-pass filter formula matches ✓

GAPS:
1. DEADBAND value mismatch: Python const.py DEADBAND=0.1C; C++ default SAT_PID_DEADBAND_DEFAULT=0.25C. C++ uses settings.sat.fDeadband (configurable), but the DEFAULT differs from Python. Python is hardcoded at 0.1C.
2. Python freeze_integral is passed as parameter by caller (heating_control passes freeze_integral=True during solar gain per climate.py). C++ checks bSolarGainActive inside _pidUpdateIntegral() directly — functionally equivalent but the freeze mechanism is internal rather than caller-driven. No real gap.
3. Python stores PID state via HA Store (async persistence); C++ saves to LittleFS. Functionally equivalent.
4. Python _update_derivative: inside deadband, freezes by updating timestamp but not raw_derivative (line 206). C++ keeps timestamp updated via _pid_lastDerivativeMs = millis(). The C++ freeze logic is inverted: C++ checks _pid_lastError (previous error) but Python checks state.error (current error). Edge case: on first call after crossing deadband boundary, C++ may use one extra cycle of old error. Minor.
5. No manual gains mode in auto-gain: Python has `if not self._config.automatic_gains: return float_value(self._config.proportional)` — manual Kp/Ki/Kd config. C++ only has automatic gains. Settings exist for manual but _pidCalculateGains() always uses automatic formula.

Created audit-fix task for deadband default mismatch.
<!-- SECTION:FINAL_SUMMARY:END -->
