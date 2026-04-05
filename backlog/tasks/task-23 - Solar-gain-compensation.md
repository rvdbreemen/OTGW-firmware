---
id: TASK-23
title: Solar gain compensation
status: To Do
assignee: []
created_date: '2026-04-05 20:46'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Detect solar heating via indoor temperature rise rate and adjust setpoint down to prevent overheating. SAT Python (solar_gain.py) uses sun elevation, minimum rise/hour threshold, and can freeze the integral during solar gain events. Requires outside temperature sensor and optionally a solar radiation or sun elevation source via MQTT.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/solar_gain.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Detect indoor temp rise rate exceeding threshold while boiler output is low
- [ ] #2 Configurable min sun elevation angle for solar gain detection
- [ ] #3 Configurable min indoor rise per hour threshold
- [ ] #4 Setpoint offset reduction during solar gain event
- [ ] #5 Freeze integral during solar gain to prevent windup
- [ ] #6 MQTT/REST/WebUI: solar gain status and settings
<!-- AC:END -->
