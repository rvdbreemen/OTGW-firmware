---
id: TASK-24
title: Summer simmer index for auto heating disable
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
Humidity + temperature comfort index that can auto-disable heating when summer conditions are met. SAT Python (summer_simmer.py) calculates a simmer index from indoor temp and humidity to determine perceived comfort.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/summer_simmer.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Calculate summer simmer index from indoor temperature and humidity
- [ ] #2 Auto-disable SAT heating when simmer index exceeds comfort threshold
- [ ] #3 Requires humidity sensor via MQTT
- [ ] #4 MQTT/REST/WebUI: simmer index value and threshold setting
<!-- AC:END -->
