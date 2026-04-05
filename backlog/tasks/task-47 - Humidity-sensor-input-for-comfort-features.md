---
id: TASK-47
title: Humidity sensor input for comfort features
status: To Do
assignee: []
created_date: '2026-04-05 21:06'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:84'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add CONF_HUMIDITY_SENSOR_ENTITY_ID configuration to accept humidity readings via MQTT input. Required by thermal comfort index and summer simmer calculations. Enables comfort-aware heating decisions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CONF_HUMIDITY_SENSOR_ENTITY_ID setting added to configuration
- [ ] #2 Humidity value received via MQTT input
- [ ] #3 Value validated (0-100% range)
- [ ] #4 Humidity available to thermal comfort and summer simmer calculations
- [ ] #5 Graceful fallback when humidity sensor unavailable
<!-- AC:END -->
