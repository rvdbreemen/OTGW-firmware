---
id: TASK-62
title: 'HA Number Entity: Hot Water Setpoint'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:12'
updated_date: '2026-04-06 20:13'
labels:
  - ha-entity
  - number
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/number.py
    (lines 24-75, SatHotWaterSetpointEntity)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatHotWaterSetpointEntity` to MQTT auto-discovery. This is a controllable number entity (device_class=temperature, 30-60C range) that allows users to adjust the DHW setpoint directly from HA. Currently our firmware has a DHW setpoint setting but it's not exposed as an interactive HA number entity that the user can slide/adjust in the dashboard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT state topic sat/dhw_setpoint published with current DHW setpoint
- [x] #2 MQTT command topic for setting DHW setpoint
- [x] #3 HA auto-discovery config for number entity with device_class=temperature, unit=C
- [x] #4 Range: 30-60C (matching SAT Python min/max)
- [x] #5 Icon: mdi:thermometer
- [x] #6 Updates persist to settings and take effect on next DHW cycle
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DHW Setpoint HA number entity with slider (30-60C, step 0.5). Publishes sat/dhw_setpoint, cmd_t uses existing MQTT subscribe handler.
<!-- SECTION:FINAL_SUMMARY:END -->
