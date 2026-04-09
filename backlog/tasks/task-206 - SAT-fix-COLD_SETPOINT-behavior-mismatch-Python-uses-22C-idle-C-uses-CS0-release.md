---
id: TASK-206
title: >-
  SAT fix: COLD_SETPOINT behavior mismatch - Python uses 22C idle, C++ uses CS=0
  release
status: To Do
assignee: []
created_date: '2026-04-09 05:23'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python uses COLD_SETPOINT=22C as the boiler setpoint when SAT is in OFF mode or valves are closed. This keeps the boiler in a warm-idle state. C++ sends CS=0 (release) or CS=10 (SAT_MIN_SETPOINT), which fully relinquishes boiler control or sets a cold setpoint. Decision needed: should C++ match Python by sending CS=22 when SAT is effectively off but still connected, or is CS=0 (thermostat resumes control) the preferred behavior for OTGW?
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Behavior is documented: intentional CS=0 (release to thermostat) or CS=22 (warm idle)
- [ ] #2 If CS=22 is chosen: summer mode, valve-closed, and DHW-priority correctly set warm idle
- [ ] #3 If CS=0 is chosen: existing behavior is preserved and documented as deliberate design choice
<!-- AC:END -->
