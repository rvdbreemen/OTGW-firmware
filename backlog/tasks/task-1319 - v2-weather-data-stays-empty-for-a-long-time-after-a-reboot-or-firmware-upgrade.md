---
id: TASK-1319
title: >-
  v2: weather data stays empty for a long time after a reboot or firmware
  upgrade
status: To Do
assignee: []
created_date: '2026-10-23T13:13:00Z'
labels:
  - sat
  - weather
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 442000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 12:44Z (#alpha-testing, screenshot): after a firmware upgrade (and probably after a reboot) the weather data takes a long time to fill. Asks whether it can be fetched right away. Check the weather fetch schedule at boot (first fetch delay vs interval) and whether WiFi/NTP readiness gates it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The first weather fetch after boot happens within a bounded time once WiFi and time are up (measured on the bench: seconds from boot to state.sat.weather.bValid)
- [ ] #2 Old vs new boot-to-weather time recorded
<!-- AC:END -->
