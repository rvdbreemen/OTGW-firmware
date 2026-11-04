---
id: TASK-206
title: >-
  SAT fix: COLD_SETPOINT behavior mismatch - Python uses 22C idle, C++ uses CS=0
  release
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:23'
updated_date: '2026-04-09 10:34'
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
- [x] #1 Behavior is documented: intentional CS=0 (release to thermostat) or CS=22 (warm idle)
- [ ] #2 If CS=22 is chosen: summer mode, valve-closed, and DHW-priority correctly set warm idle
- [x] #3 If CS=0 is chosen: existing behavior is preserved and documented as deliberate design choice
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Find CS=0 release in satDisable
2. Add explanatory comment
3. Document as deliberate design choice
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Documented CS=0 (release to thermostat) as the deliberate OTGW design choice when SAT is disabled. Python SAT uses CS=22 warm-idle because it is a standalone HA thermostat replacement; OTGW firmware is a gateway sitting between thermostat and boiler, so it defers to the physical thermostat instead. Added a 5-line explanatory comment in satDisable() in SATcontrol.ino at the addCommandToQueue(CS=0) call site, replacing the previous single-line comment. No logic changed -- existing behavior is correct and preserved.
<!-- SECTION:FINAL_SUMMARY:END -->
