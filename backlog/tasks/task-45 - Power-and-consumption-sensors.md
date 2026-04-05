---
id: TASK-45
title: Power and consumption sensors
status: To Do
assignee: []
created_date: '2026-04-05 21:06'
labels:
  - sat
  - feature
dependencies: []
references:
  - other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Calculate and expose power (kW) from modulation percentage * boiler capacity, and cumulative energy consumption (kWh) tracking with min/max bounds. Useful for energy dashboards and Home Assistant energy integration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Power sensor (kW) calculated from modulation * capacity
- [ ] #2 Cumulative energy consumption sensor (kWh) with running total
- [ ] #3 Min/max bounds applied to prevent erroneous readings
- [ ] #4 Sensors exposed via MQTT
- [ ] #5 Compatible with Home Assistant energy dashboard
<!-- AC:END -->
