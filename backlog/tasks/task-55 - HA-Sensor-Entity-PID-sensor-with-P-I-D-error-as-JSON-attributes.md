---
id: TASK-55
title: 'HA Sensor Entity: PID sensor with P/I/D/error as JSON attributes'
status: To Do
assignee: []
created_date: '2026-04-06 19:11'
labels:
  - ha-entity
  - sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
    (lines 101-181, SatPidSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Restructure PID data publishing to match SAT Python's `SatPidSensor` pattern. In SAT Python, PID is a single sensor entity (device_class=temperature, unit=C) whose main state is pid.output, with error/proportional/integral/derivative as extra_state_attributes. Currently the firmware publishes these as separate MQTT topics. Add a JSON attributes topic so HA can display them as attributes of a single PID sensor entity.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 HA auto-discovery config for PID sensor with device_class=temperature, unit=C
- [ ] #2 Main state topic: sat/pid_output (already exists)
- [ ] #3 JSON attributes topic: sat/pid_attributes with {error, proportional, integral, derivative}
- [ ] #4 Existing separate topics (sat/pid_p, sat/pid_i, sat/pid_d, sat/error) remain for backwards compatibility
- [ ] #5 Per-area PID sensors when multi-area is enabled (sat/area/N/pid_output + attributes)
<!-- AC:END -->
