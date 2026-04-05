---
id: TASK-10
title: Boiler pressure monitoring and alarm
status: To Do
assignee: []
created_date: '2026-04-05 10:06'
updated_date: '2026-04-05 21:07'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
  - safety
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python monitors boiler water pressure (CHPressure from OT MsgID 18) and detects low pressure, high pressure, and rapid pressure drops (possible leaks). On the ESP this data is already available via OTcurrentSystemState.CHPressure. Implement pressure monitoring with configurable thresholds and EMA smoothing. On threshold breach: publish alarm via MQTT and WebUI. No active action (no CS=0 on pressure problem) - signaling only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New settings: fMinPressure (default 0.8), fMaxPressure (default 2.5), fMaxPressureDropRate (default 0.3 bar/hour)
- [ ] #2 EMA smoothing on pressure readings (alpha=0.05 per SAT Python)
- [ ] #3 State tracking: state.sat.fSmoothedPressure, state.sat.bPressureAlarm
- [ ] #4 Pressure drop detection: calculate bar/hour based on EMA smoothed readings
- [ ] #5 Alarm after 120s persistent condition (prevent false positives)
- [ ] #6 REST API: GET /api/v2/sat/status includes pressure, smoothed_pressure, pressure_alarm, pressure_drop_rate fields
- [ ] #7 MQTT publish: sat/pressure, sat/pressure_alarm, sat/pressure_drop_rate
- [ ] #8 WebUI: pressure indicator in SAT dashboard with color coding (green/yellow/red)
- [ ] #9 HA auto-discovery: binary_sensor for pressure_alarm, sensor for pressure in mqttha.cfg
- [ ] #10 Settings persistence via settingStuff.ino
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Advanced pressure analytics (EMA smoothing, linear regression drop-rate, settle delay, confirmation window) have been split into a separate dedicated task: TASK-39 "Pressure health: advanced EMA + regression analytics". This task (TASK-10) covers the basic pressure monitoring threshold. See TASK-39 for the advanced implementation.
<!-- SECTION:NOTES:END -->
