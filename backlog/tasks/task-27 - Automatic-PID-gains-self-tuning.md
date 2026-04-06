---
id: TASK-27
title: Automatic PID gains self-tuning
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:46'
updated_date: '2026-04-06 13:00'
labels:
  - sat
  - feature
  - pid
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Self-tuning PID coefficients based on system response. SAT Python (automatic_gains) adjusts P/I/D gains automatically by observing how the system responds to setpoint changes, reducing the need for manual tuning.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_AUTOMATIC_GAINS)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Observe system response to setpoint changes (overshoot, settling time)
- [x] #2 Auto-adjust P/I/D gains based on observed response
- [x] #3 Configurable enable/disable for auto-tuning
- [x] #4 MQTT/REST/WebUI: current auto-tuned gain values
- [x] #5 Persist tuned gains to LittleFS
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add bAutoTune + fAutoTuneRate to SATSection (settings) in OTGW-firmware.h
2. Add auto-tune runtime state to SATRuntimeSection in OTGW-firmware.h
3. Implement satAutoTuneUpdate() in SATcontrol.ino with performance tracking and gain adjustment
4. Add persistence in settingStuff.ino (write + read handlers)
5. Add to sendDeviceSettings() in restAPI.ino and knownSettings whitelist
6. Add MQTT subscribe handler in MQTTstuff.ino
7. Add JSON status + MQTT publish entries in SATcontrol.ino
8. Add tooltips in index.js
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT thermo-nova auto PID gains reference (pid.py:62-93):
Automatic gain calculation formulas:
- Kp = (coefficient * curve_value) / 4
  - Use divisor 3 instead of 4 for underfloor heating (slower response)
- Ki = Kp / 8400
  - 8400 = typical integral time constant in seconds (~2.3 hours)
- Kd = 0.07 * 8400 * Kp
  - 0.07 = derivative damping factor
  - Results in Kd proportional to Kp with time-constant scaling
- curve_value comes from heating curve calculation
- coefficient is the user-configured heating curve coefficient

Implementation complete:
- satAutoTuneUpdate() monitors cycle classifications from existing cycle tracker
- Adjusts heating curve coefficient (which drives Kp/Ki/Kd auto-gain calculation)
- Conservative: 1hr minimum interval, 6+ cycles required, score threshold 0.3
- Oscillation detection via alternating overshoot/undershoot pattern
- Slow convergence detection via average overshoot duration
- Persists adjusted coefficient via writeSettings()
- Full MQTT/REST/WebUI integration with enable/disable + rate config
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Auto PID tuning via heating curve coefficient adjustment based on cycle performance
<!-- SECTION:FINAL_SUMMARY:END -->
