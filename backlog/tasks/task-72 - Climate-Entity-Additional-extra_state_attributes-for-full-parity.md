---
id: TASK-72
title: 'Climate Entity: Additional extra_state_attributes for full parity'
status: To Do
assignee: []
created_date: '2026-04-06 19:14'
labels:
  - ha-entity
  - climate
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 236-263, extra_state_attributes property)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add the remaining SAT Python climate entity extra_state_attributes that are not yet covered by other tasks. SAT Python exposes 35 attributes on the climate entity. This task covers the attributes not addressed by tasks #64-67: optimal_coefficient, coefficient_derivative, minimum_setpoint, boiler_flame_timing, boiler_temperature_cold, boiler_temperature_tracking, boiler_temperature_derivative, error_source, error_pid, integral_enabled, derivative_enabled, derivative_raw, current_kp/ki/kd, relative_modulation_enabled. Most of these map to existing firmware state fields but need to be published as a JSON attributes blob.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 JSON attributes topic sat/climate_attributes published with all remaining attributes
- [ ] #2 Includes: optimal_coefficient, coefficient_derivative from heating curve auto-tune
- [ ] #3 Includes: minimum_setpoint (dynamic or static current value)
- [ ] #4 Includes: boiler_flame_timing (average flame on-time seconds)
- [ ] #5 Includes: boiler_temperature_cold, boiler_temperature_tracking, boiler_temperature_derivative
- [ ] #6 Includes: error_source (which zone has max error), error_pid, integral_enabled, derivative_enabled
- [ ] #7 Includes: derivative_raw, current_kp, current_ki, current_kd
- [ ] #8 Includes: relative_modulation_enabled (bool)
- [ ] #9 Climate HA auto-discovery uses json_attributes_topic for these
<!-- AC:END -->
