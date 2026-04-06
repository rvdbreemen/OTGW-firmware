---
id: TASK-21
title: Thermal drop learning for SAT fallback mode
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 11:45'
updated_date: '2026-04-06 12:34'
labels:
  - sat
  - feature
  - safety
  - persistence
dependencies:
  - TASK-19
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When SAT operates in fallback mode (external control lost), it should make intelligent decisions about heating even without real-time room temperature data. The firmware can learn the house's thermal characteristics over time: how fast does the indoor temperature drop relative to the outside temperature? This 'thermal drop rate' (degrees/hour at a given delta-T) allows SAT to estimate room temperature decay and maintain reasonable heating during a fallback event. This is explicitly a safety fallback, not a permanent solution - real temperature sensor input is always preferred. The learned thermal model is stored persistently and updated continuously during normal operation when real sensor data is available.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Track temperature drop rate during heating-off periods: delta_indoor / delta_time at known delta_outdoor
- [x] #2 Store learned thermal coefficient in settings (degrees/hour per degree indoor-outdoor delta)
- [x] #3 During fallback without room temp: estimate current room temp using last known temp + learned drop rate + elapsed time + outside temp
- [x] #4 Estimated temp has increasing uncertainty over time - widen deadband proportionally
- [x] #5 After 2+ hours of estimation without real data, switch to fixed safe setpoint (e.g. 45C flow) instead of PID
- [x] #6 Learning only runs when SAT has valid room temp AND outside temp simultaneously
- [x] #7 Minimum 24h of data before thermal model is considered valid
- [x] #8 State tracking: state.sat.fThermalDropRate, state.sat.bThermalModelValid
- [x] #9 REST API: GET /api/v2/sat/status includes thermal_drop_rate and thermal_model_valid
- [x] #10 MQTT publish: sat/thermal_drop_rate
- [x] #11 Settings persistence: learned coefficient saved to LittleFS
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add fThermalCoeff to SATSection (settings) and thermal state fields to SATRuntimeSection
2. Add thermal learning constants and static tracking variables to SATcontrol.ino
3. Implement satUpdateThermalLearning() - learns drop rate during flame-off periods using EMA
4. Implement satEstimateRoomTemp() - estimates room temp using learned coefficient during fallback
5. Modify satGetRoomTemp() to use thermal estimation when in fallback with invalid room temp
6. Add deadband widening (AC#4) and safe setpoint fallback after 2h (AC#5) in satControlLoop
7. Add thermal fields to satSendStatusJSON() and satPublishMQTT()
8. Add persistence in settingStuff.ino (write + update handler)
9. Add to restAPI.ino sendDeviceSettings + knownSettings
10. Add translateSettings + tooltip in index.js
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Thermal drop learning for SAT fallback mode.

Changes:
- Learns building thermal decay rate during flame-off periods (EMA, alpha=0.1)
- Estimates room temp in fallback: lastKnown - coeff * deltaT * hours
- 24h minimum learning before model is valid
- Widens deadband during estimation (0.5C/hr safety)
- Falls back to fixed 45C safe setpoint after 2h without real data
- Coefficient saved hourly, constrained 0.005-0.3
- MQTT: sat/thermal_coeff, sat/estimated_room
<!-- SECTION:FINAL_SUMMARY:END -->
