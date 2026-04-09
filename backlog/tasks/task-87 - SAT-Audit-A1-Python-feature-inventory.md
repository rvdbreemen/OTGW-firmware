---
id: TASK-87
title: 'SAT Audit A1: Python feature inventory'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:48'
updated_date: '2026-04-09 05:28'
labels:
  - audit
  - sat
  - phase-1
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Create a complete inventory of all features in the SAT Python integration (thermo-nova branch). List all classes, methods and configuration options as the baseline for the rest of the audit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All Python classes and their responsibilities are documented
- [x] #2 All configuration parameters listed with type and default value
- [x] #3 All HA service calls and states inventoried
- [x] #4 Result published as a structured markdown document in backlog/docs/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed full inventory of all Python classes, methods, and configuration parameters in the SAT thermo-nova integration.

Published: backlog/docs/sat-python-feature-inventory.md

Coverage:
- 11 modules inventoried: pid.py, heating_curve.py, pwm.py, heating_control.py, solar_gain.py, summer_simmer.py, climate.py, entry_data.py, types.py, helpers.py, coordinator/, cycles/, device/, area.py, manufacturer.py, sensor.py, binary_sensor.py
- All 39 OPTIONS_DEFAULTS config parameters documented with types and defaults
- All 16 BoilerStatus states, 8 CycleClassification values, all domain enums documented
- All HA services inventoried: reset_integral, set_overshoot_protection_value, start_overshoot_protection_calculation
- Key algorithms documented: PID auto-gain formulas, PWM 4-region duty cycle, heating curve formula, solar gain detection, Summer Simmer Index, multi-area 75th-percentile aggregation, pressure health linear regression

This document serves as the authoritative baseline for audit phases B-E.
<!-- SECTION:FINAL_SUMMARY:END -->
