---
id: TASK-10
title: Boiler pressure monitoring and alarm
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:06'
updated_date: '2026-04-05 23:24'
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
- [x] #1 New settings: fMinPressure (default 0.8), fMaxPressure (default 2.5), fMaxPressureDropRate (default 0.3 bar/hour)
- [x] #2 EMA smoothing on pressure readings (alpha=0.05 per SAT Python)
- [x] #3 State tracking: state.sat.fSmoothedPressure, state.sat.bPressureAlarm
- [x] #4 Pressure drop detection: calculate bar/hour based on EMA smoothed readings
- [x] #5 Alarm after 120s persistent condition (prevent false positives)
- [x] #6 REST API: GET /api/v2/sat/status includes pressure, smoothed_pressure, pressure_alarm, pressure_drop_rate fields
- [x] #7 MQTT publish: sat/pressure, sat/pressure_alarm, sat/pressure_drop_rate
- [ ] #8 WebUI: pressure indicator in SAT dashboard with color coding (green/yellow/red)
- [x] #9 HA auto-discovery: binary_sensor for pressure_alarm, sensor for pressure in mqttha.cfg
- [x] #10 Settings persistence via settingStuff.ino
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Advanced pressure analytics (EMA smoothing, linear regression drop-rate, settle delay, confirmation window) have been split into a separate dedicated task: TASK-39 "Pressure health: advanced EMA + regression analytics". This task (TASK-10) covers the basic pressure monitoring threshold. See TASK-39 for the advanced implementation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Boiler pressure monitoring with EMA smoothing and alarm.\n\nChanges:\n- EMA smoothing (alpha=0.05 per SAT Python)\n- Drop rate calculation (bar/hour) from smoothed readings\n- Alarm after 120s persistent low/high/rapid-drop condition\n- Settings: fMinPressure, fMaxPressure, fMaxPressureDrop\n- REST API: pressure, smoothed_pressure, pressure_alarm, pressure_drop_rate\n- MQTT: sat/pressure, sat/pressure_alarm, sat/pressure_drop_rate\n- HA auto-discovery: pressure sensor + pressure_alarm binary_sensor\n- AC#8 (WebUI color coding) left for WebUI polish phase\n\nFiles: OTGW-firmware.h, SATcontrol.ino, settingStuff.ino, mqttha.cfg
<!-- SECTION:FINAL_SUMMARY:END -->
