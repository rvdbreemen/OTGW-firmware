---
id: TASK-29
title: Climate valve offset (TRV integration)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:47'
updated_date: '2026-04-06 20:59'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Adjust SAT setpoint based on TRV (thermostatic radiator valve) positions reported via MQTT. When valves are mostly closed, reduce setpoint. When valves are wide open, increase. SAT Python uses climate_valve_offset config.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_CLIMATE_VALVE_OFFSET)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Read TRV valve positions from MQTT
- [x] #2 Calculate setpoint offset based on average valve openness
- [x] #3 Configurable offset scaling factor
- [x] #4 MQTT/REST/WebUI: valve positions and calculated offset
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Optie A: MQTT-input benadering voor TRV valve detection
1. Add MQTT subscribe handler for set/.../sat/valves_open (bool)
2. Add state.sat.bValvesOpen field to track valve status
3. When valves_open=false: stop heater, freeze PID integral, skip error recording
4. Publish sat/valves_open as binary sensor
5. Add HA discovery entry
6. Document HA template sensor example for users
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TRV valve detection via MQTT input (3ad215ce).
- MQTT subscribe: set/.../sat/valves_open (true/false/1/0/open/closed)
- When valves closed: sends CS=0, skips PID update and error recording
- Default: true (assume heat demand)
- HA binary_sensor discovery with mdi:radiator icon
- REST API includes valves_open in status JSON
<!-- SECTION:FINAL_SUMMARY:END -->
