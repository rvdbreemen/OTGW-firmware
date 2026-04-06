---
id: TASK-23
title: Solar gain compensation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:46'
updated_date: '2026-04-06 12:19'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Detect solar heating via indoor temperature rise rate and adjust setpoint down to prevent overheating. SAT Python (solar_gain.py) uses sun elevation, minimum rise/hour threshold, and can freeze the integral during solar gain events. Requires outside temperature sensor and optionally a solar radiation or sun elevation source via MQTT.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/solar_gain.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Detect indoor temp rise rate exceeding threshold while boiler output is low
- [x] #2 Configurable min indoor rise per hour threshold
- [x] #3 Setpoint offset reduction during solar gain event
- [x] #4 Freeze integral during solar gain to prevent windup
- [x] #5 MQTT/REST/WebUI: solar gain status and settings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add solar gain settings (enable, min rise rate C/hr, setpoint offset)
2. Track indoor temp rise rate over sliding window
3. Detect solar gain: temp rising while boiler modulation is low
4. Apply setpoint offset during solar gain event
5. Freeze PID integral during solar gain
6. MQTT/REST/WebUI status
7. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Added settings: bSolarGainEnable, fSolarMinRiseRate, fSolarSetpointOffset to SATSection
- Added state: bSolarGainActive, fIndoorRiseRate to SATRuntimeSection
- Implemented satUpdateSolarGain() with EMA-smoothed rise rate, 10min hysteresis for both activation and deactivation
- Applied setpoint offset in satControlLoop after final clamp
- Added integral freeze guard in SATpid.ino _pidUpdateIntegral
- Added persistence in settingStuff.ino (write + load)
- Added REST API settings in restAPI.ino (sendDeviceSettings + knownSettings)
- Added MQTT publishing and JSON status entries
- Added translateSettings labels and tooltips in index.js
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Solar gain compensation: detects indoor temp rise from sunlight.

Changes:
- EMA-smoothed indoor rise rate detection (C/hr)
- Triggers when rise rate > threshold AND boiler modulation < 30%
- 10-min hysteresis for both activation and deactivation
- Setpoint offset reduction during solar gain event
- PID integral freeze to prevent windup
- Settings: SATsolargain, SATsolarminrise, SATsolaroffset
- MQTT: sat/solar_gain, REST/JSON status
<!-- SECTION:FINAL_SUMMARY:END -->
