---
id: TASK-37
title: Simulation mode for testing without boiler
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
Simulation mode that emulates boiler behavior for testing SAT control logic without a real boiler connected. SAT Python has simulated_heating, simulated_cooling, and simulated_warming_up configs that model thermal behavior.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_SIMULATED_*)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Simulated heating: model temperature rise when flame on
- [ ] #2 Simulated cooling: model temperature drop when flame off
- [ ] #3 Simulated warming up: model initial heating phase
- [ ] #4 Configurable enable/disable simulation mode
- [ ] #5 WebUI: simulation mode toggle and simulated temp display
<!-- AC:END -->
