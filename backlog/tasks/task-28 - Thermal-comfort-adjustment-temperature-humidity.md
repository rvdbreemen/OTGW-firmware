---
id: TASK-28
title: Thermal comfort adjustment (temperature + humidity)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:46'
updated_date: '2026-04-06 12:40'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Adjust SAT target based on perceived thermal comfort combining temperature and humidity. Higher humidity makes the same temperature feel warmer, so the heating setpoint can be lowered. SAT Python uses a thermal_comfort config option.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_THERMAL_COMFORT)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Calculate perceived comfort from indoor temp + humidity
- [x] #2 Adjust heating setpoint based on comfort index
- [x] #3 Requires humidity sensor via MQTT
- [x] #4 Configurable enable/disable and sensitivity
- [x] #5 MQTT/REST/WebUI: comfort index and adjustment values
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Thermal comfort adjustment from humidity, applied as target temp offset
<!-- SECTION:FINAL_SUMMARY:END -->
