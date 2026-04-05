---
id: TASK-36
title: Reset integral service (debug tool)
status: To Do
assignee: []
created_date: '2026-04-05 20:52'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Provide a way to manually reset the PID integral value. Useful for debugging and when the integral has accumulated incorrectly. SAT Python exposes this as an HA service call.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/services.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 REST API: POST /api/v2/sat/reset_integral to reset integral to 0
- [ ] #2 MQTT command: set/<nodeId>/sat/reset_integral
- [ ] #3 WebUI: reset integral button in PID debug section
<!-- AC:END -->
