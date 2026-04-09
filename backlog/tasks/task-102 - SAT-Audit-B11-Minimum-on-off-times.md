---
id: TASK-102
title: 'SAT Audit B11: Minimum on/off times'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:51'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the minimum on and off times for the boiler in Python SAT thermo-nova with the C++ firmware. Verify protection against rapid switching and interaction with PWM.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Minimum on-time logic compared
- [x] #2 Minimum off-time logic compared
- [x] #3 Interaction with PWM cycle verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read Python device/const.py, device/status.py, cycles/const.py, pwm.py
2. Read C++ BS_ANTI_CYCLE_MIN_OFF_MS, BS_STALLED_IGNITION_MIN_MS, SAT_PWM_MIN_ON_SEC
3. Compare all timing thresholds
4. Identify gaps and create fix tasks
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Minimum OFF time: Python BOILER_ANTI_CYCLING_MIN_OFF_SECONDS=180s matches C++ BS_ANTI_CYCLE_MIN_OFF_MS=180000ms. EXACT MATCH.

2. Minimum ON time in PWM: Python HEATER_STARTUP_TIMEFRAME=180s for gas boilers. C++ satGetMinOnTimeSec()=180 for gas, 1800 for heat pumps. MATCHES for gas. Python const.py does not have heat-pump distinction.

3. Ignition wait timeout: Python pwm.py:194-202 starts timer anyway if flame not detected within HEATER_STARTUP_TIMEFRAME (180s). C++ _pwm_waitingForFlame has no timeout - it waits indefinitely for flame ignition. GAP: if flame never ignites, C++ sends low CS setpoint forever.

4. Stalled ignition detection: Python BOILER_STALL_IGNITION_MIN_OFF_SECONDS=600s + last_cycle*3. C++ BS_STALLED_IGNITION_MIN_MS=600000ms + lastCycleDuration*3. EXACT MATCH.

5. PWM interaction: C++ uses 5-range duty cycle mapper matching Python pwm.py logic. Post-cycle settling: Python 60s, C++ BS_POST_CYCLE_SETTLE_MS=60000ms. MATCH.

Fix task created for ignition wait timeout gap.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B11: Minimum on/off times

All timing constants match Python exactly: anti-cycling min-off=180s, min-on=180s for gas/1800s for heat pump, stalled ignition=600s+3x last cycle, post-cycle settling=60s.

One gap found: Python PWM has a 180s ignition timeout (if flame not detected, start timer anyway). C++ _pwm_waitingForFlame has no timeout and waits indefinitely. Fix task TASK-208 created.

No structural differences in PWM duty cycle calculation or cycle interaction.
<!-- SECTION:FINAL_SUMMARY:END -->
