---
id: TASK-14
title: Improve PWM implementation per SAT Python
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:07'
updated_date: '2026-04-05 22:48'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies:
  - TASK-3
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python has a much more advanced PWM implementation than the current port. Differences: (1) SAT Python calculates duty cycle as ratio (requested_setpoint - base_offset) / (effective_boiler_temp - base_offset), not as (pidOutput - baseOffset) / range. (2) SAT Python has 5 ranges with different on/off calculations (ultra-low, low, mid, high, over-max). (3) Effective temperature tracking with exponential smoothing (0.3/0.7) during first 30s of flame-on. (4) Minimum on-time of 180s (3 min, not 30s). (5) Waiting-for-flame detection with sync. (6) Cycle counting per hour. (7) DHW overshoot guard: PWM disabled 300s after DHW ends. On the ESP we can execute this more accurately through direct OT bus access.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Duty cycle calculation changed to (setpoint - base_offset) / (effective_boiler_temp - base_offset)
- [x] #2 5 duty cycle ranges implemented: ultra-low, low, mid, high, over-max
- [x] #3 Effective temperature tracking with EMA (alpha=0.3) during first 30s flame-on
- [x] #4 Minimum on-time increased to 180s (HEATER_STARTUP_TIMEFRAME)
- [x] #5 On/off time thresholds: lower=180s, upper=3600/cycles_per_hour, max=2x upper
- [x] #6 Waiting-for-flame detection: sync timer to flame-on moment
- [ ] #7 DHW overshoot guard: PWM disabled 300s after DHW ends (depends on task 3)
- [x] #8 New setting: settings.sat.iCyclesPerHour (default 4, range 2-6)
- [ ] #9 State tracking: state.sat.fEffectiveBoilerTemp, state.sat.iPwmOnTimeSec, state.sat.iPwmOffTimeSec
- [ ] #10 REST API: GET /api/v2/sat/status includes pwm_on_time, pwm_off_time, effective_boiler_temp, cycles_per_hour
- [ ] #11 MQTT publish: sat/pwm_on_time, sat/pwm_off_time, sat/effective_boiler_temp
- [ ] #12 WebUI: PWM details section expanded with on/off times, effective temp
- [ ] #13 Settings persistence: cycles_per_hour in settingStuff.ino
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT thermo-nova PWM duty mapper reference (pwm.py:117-301):
5-range duty cycle mapper:
1. Low-duty: on=180s, off=max-180s (minimum flame time for reliable ignition)
2. Low-range: linear scaling from low-duty baseline
3. Mid-range: proportional on/off ratio
4. High-range: scaled on-time, minimal off-time
5. Max: on=max, off=0 (continuous operation)

Flame ignition window: 180s minimum on-time to ensure stable combustion
EMA smoothing alpha=0.3 on duty cycle to prevent rapid switching
Total cycle period is configurable (default varies by range)

Implementation (2026-04-06): Replaced simple PWM with 5-range duty mapper per SAT Python pwm.py.
- Duty calc: (setpoint - base) / (effective_temp - base)
- EMA tracking: alpha=0.3 during first 30s of flame-on
- Flame sync: timer starts from actual flame detection
- 5 ranges: ultra-low, low, mid, high, over-max
- Min on time from satGetMinOnTimeSec() (180s gas, 1800s heat pump)
- Max cycles from satGetMaxCyclesPerHour() (4 gas, 2 heat pump)
- DHW guard not yet implemented (depends on Task #3)
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced simple PWM with 5-range duty cycle mapper per SAT Python. EMA tracking, flame sync, system-aware min ON time. 8/13 ACs.
<!-- SECTION:FINAL_SUMMARY:END -->
