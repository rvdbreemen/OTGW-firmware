---
id: TASK-587
title: 'feat(sat): named DS18B20 sensor to SAT area mapping in settings UI'
status: To Do
assignee: []
created_date: '2026-05-08 16:02'
labels:
  - sat
  - sensors
  - settings
  - ui
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 2.0.0 firmware supports multi-area SAT control with up to 4 area temperatures (satSetAreaTemp(area, temp)) and can use Dallas DS18B20 sensors for room temperature. However, there is no UI or settings path to map a specific DS18B20 sensor address to a SAT area. Users must wire this externally via MQTT or REST API calls. Add a persistent mapping from 1-Wire sensor address to SAT area index, configurable from the Settings UI and auto-applied at each polling cycle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 settings.sat gains up to 4 area-sensor mappings: sSensorArea[0-3] (char[17], Dallas address string or empty)
- [ ] #2 When a DS18B20 is polled and its address matches a mapping, the temperature is automatically forwarded to satSetAreaTemp(areaIdx, temp)
- [ ] #3 The Settings UI SAT section shows a per-area dropdown to select from discovered Dallas sensors (by label if set, by address otherwise)
- [ ] #4 Mappings are saved to settings.ini and restored at boot
- [ ] #5 An empty mapping (default) means no auto-routing; existing behavior is preserved
- [ ] #6 The feature works alongside existing MQTT/REST area-temp injection (last-write wins)
<!-- AC:END -->
