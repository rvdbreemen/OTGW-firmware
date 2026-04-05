---
id: TASK-25
title: Multi-area room support with weighted temperature
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
Support multiple climate zones with weighted temperature averaging and valve position integration. SAT Python (area.py) allows each room to have its own sensor and climate entity, with configurable weights based on valve positions.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/area.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Support multiple room temperature sensors via MQTT
- [ ] #2 Weighted average calculation based on configurable per-room weights
- [ ] #3 Optional valve position integration for weight adjustment
- [ ] #4 MQTT/REST/WebUI: per-room temps, weights, and combined average
<!-- AC:END -->
