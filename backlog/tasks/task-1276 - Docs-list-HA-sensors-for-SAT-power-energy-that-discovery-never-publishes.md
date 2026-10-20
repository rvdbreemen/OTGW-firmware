---
id: TASK-1276
title: Docs list HA sensors for SAT power/energy that discovery never publishes
status: To Do
assignee: []
created_date: '2026-10-20 20:34'
labels:
  - docs
  - mqtt
  - ha
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 406000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1258 review: docs/manuals/en/ch05-sat-thermostat.md ~602 and nl/h05 ~647 list sensor.otgw_sat_power (and related SAT energy sensors) as Home Assistant entities, but MQTTHaDiscovery.cpp has no discovery entry for sat/power, sat/energy_total or sat/energy_estimated_kwh (grep 2026-10-20). Either add discovery entities (power kW measurement, energy kWh total_increasing for the HA Energy dashboard) or correct the docs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded: add discovery entities or correct the docs
- [ ] #2 The chosen side matches the other (discovery payload capture or doc diff)
<!-- AC:END -->
