---
id: TASK-31
title: Push setpoint to thermostat display (TC= command)
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
Sync SAT target temperature back to the physical thermostat display using the TC= (thermostat control) OTGW command. This way the thermostat shows what SAT is actually targeting, not its own stale setpoint. SAT Python has push_setpoint_to_thermostat config option.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_PUSH_SETPOINT_TO_THERMOSTAT)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Send TC= command to update thermostat display with SAT target temp
- [ ] #2 Configurable enable/disable (default off)
- [ ] #3 Only active in gateway mode (thermostat present)
- [ ] #4 MQTT/REST/WebUI: setting to enable/disable push
<!-- AC:END -->
