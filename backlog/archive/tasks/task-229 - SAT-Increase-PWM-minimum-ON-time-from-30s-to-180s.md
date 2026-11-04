---
id: TASK-229
title: 'SAT: Increase PWM minimum ON time from 30s to 180s'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:30'
updated_date: '2026-04-09 06:15'
labels:
  - audit-fix
  - sat
  - pwm
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcontrol.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's PWM controller uses HEATER_STARTUP_TIMEFRAME = 180 seconds as the minimum ON time before the duty cycle calculation can end a PWM on-phase. The C++ firmware uses SAT_PWM_MIN_ON_SEC = 30 seconds.

This discrepancy matters because many boilers need 60-120 seconds to stabilize after ignition. Cutting the on-phase after only 30 seconds can cause repeated ignition cycles without the boiler ever reaching setpoint (short cycling), wasting gas and increasing boiler wear.

The 180s Python value was chosen based on the HEATER_STARTUP_TIMEFRAME constant which is explicitly named for the heater startup phase. The 30s C++ value was likely chosen conservatively during initial porting without validating against real-world boiler behavior.

Complexity: Trivial. Single constant change. However, must check whether the 30s value was intentional for any reason (e.g. heat pump support where startup is faster).

Risk: Low. Increasing the minimum ON time means longer cycles in edge cases, but this is what most boilers prefer. The change should be validated against the existing cycle auto-switch logic to ensure the 60s overshoot timer still fires correctly even with the longer minimum ON time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT_PWM_MIN_ON_SEC is increased from 30 to 180 seconds
- [x] #2 PWM on-phase is not terminated before 180 seconds have elapsed regardless of duty cycle
- [x] #3 Existing auto-switch logic (60s sustained overshoot -> enable PWM) remains unaffected
- [x] #4 Change is documented in a code comment referencing the Python HEATER_STARTUP_TIMEFRAME constant
- [x] #5 No regression on PWM on/off state machine transitions
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify SAT_PWM_MIN_ON_SEC (30s) is declared but unused (actual code uses satGetMinOnTimeSec() = 180s for gas)
2. Update the dead constant to 180s with comment referencing Python HEATER_STARTUP_TIMEFRAME
3. Verify no regression: satGetMinOnTimeSec() already returns 180 for gas/underfloor, 1800 for heat pumps
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated SAT_PWM_MIN_ON_SEC from 30s to 180s to match Python HEATER_STARTUP_TIMEFRAME. The constant was declared but not used (satGetMinOnTimeSec() already returns 180s for gas boilers); updated for consistency and added explanatory comment. No behavioral change in gas/underfloor systems; heat pump path (1800s) remains unaffected.
<!-- SECTION:FINAL_SUMMARY:END -->
