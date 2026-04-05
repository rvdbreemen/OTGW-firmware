---
id: TASK-29
title: Climate valve offset (TRV integration)
status: To Do
assignee: []
created_date: '2026-04-05 20:47'
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
- [ ] #1 Read TRV valve positions from MQTT
- [ ] #2 Calculate setpoint offset based on average valve openness
- [ ] #3 Configurable offset scaling factor
- [ ] #4 MQTT/REST/WebUI: valve positions and calculated offset
<!-- AC:END -->
