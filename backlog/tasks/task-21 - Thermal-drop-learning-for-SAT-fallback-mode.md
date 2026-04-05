---
id: TASK-21
title: Thermal drop learning for SAT fallback mode
status: To Do
assignee: []
created_date: '2026-04-05 11:45'
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
- [ ] #1 Track temperature drop rate during heating-off periods: delta_indoor / delta_time at known delta_outdoor
- [ ] #2 Store learned thermal coefficient in settings (degrees/hour per degree indoor-outdoor delta)
- [ ] #3 During fallback without room temp: estimate current room temp using last known temp + learned drop rate + elapsed time + outside temp
- [ ] #4 Estimated temp has increasing uncertainty over time - widen deadband proportionally
- [ ] #5 After 2+ hours of estimation without real data, switch to fixed safe setpoint (e.g. 45C flow) instead of PID
- [ ] #6 Learning only runs when SAT has valid room temp AND outside temp simultaneously
- [ ] #7 Minimum 24h of data before thermal model is considered valid
- [ ] #8 State tracking: state.sat.fThermalDropRate, state.sat.bThermalModelValid
- [ ] #9 REST API: GET /api/v2/sat/status includes thermal_drop_rate and thermal_model_valid
- [ ] #10 MQTT publish: sat/thermal_drop_rate
- [ ] #11 Settings persistence: learned coefficient saved to LittleFS
<!-- AC:END -->
