---
id: TASK-229
title: 'SAT: Increase PWM minimum ON time from 30s to 180s'
status: To Do
assignee: []
created_date: '2026-04-09 05:30'
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
- [ ] #1 SAT_PWM_MIN_ON_SEC is increased from 30 to 180 seconds
- [ ] #2 PWM on-phase is not terminated before 180 seconds have elapsed regardless of duty cycle
- [ ] #3 Existing auto-switch logic (60s sustained overshoot -> enable PWM) remains unaffected
- [ ] #4 Change is documented in a code comment referencing the Python HEATER_STARTUP_TIMEFRAME constant
- [ ] #5 No regression on PWM on/off state machine transitions
<!-- AC:END -->
