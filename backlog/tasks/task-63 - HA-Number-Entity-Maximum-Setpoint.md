---
id: TASK-63
title: 'HA Number Entity: Maximum Setpoint'
status: To Do
assignee: []
created_date: '2026-04-06 19:12'
labels:
  - ha-entity
  - number
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/number.py
    (lines 77-128, SatMaximumSetpointEntity)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatMaximumSetpointEntity` to MQTT auto-discovery. This is a controllable number entity (device_class=temperature, 30-80C range) that allows users to set the maximum boiler setpoint from the HA dashboard. This limits how high SAT can push the boiler temperature. Important safety feature for underfloor heating (typically max 45C) vs radiators (up to 80C).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT state topic sat/max_setpoint published with current maximum setpoint
- [ ] #2 MQTT command topic for setting maximum setpoint
- [ ] #3 HA auto-discovery config for number entity with device_class=temperature, unit=C
- [ ] #4 Range: 30-80C (matching SAT Python min/max)
- [ ] #5 Icon: mdi:thermometer
- [ ] #6 Updates persist to settings and immediately affect SAT control loop clamping
<!-- AC:END -->
