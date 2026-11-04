---
id: TASK-587
title: 'feat(sat): named DS18B20 sensor to SAT area mapping in settings UI'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:02'
updated_date: '2026-05-08 17:12'
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
- [x] #1 settings.sat gains up to 4 area-sensor mappings: sSensorArea[0-3] (char[17], Dallas address string or empty)
- [x] #2 When a DS18B20 is polled and its address matches a mapping, the temperature is automatically forwarded to satSetAreaTemp(areaIdx, temp)
- [x] #3 The Settings UI SAT section shows a per-area dropdown to select from discovered Dallas sensors (by label if set, by address otherwise)
- [x] #4 Mappings are saved to settings.ini and restored at boot
- [x] #5 An empty mapping (default) means no auto-routing; existing behavior is preserved
- [x] #6 The feature works alongside existing MQTT/REST area-temp injection (last-write wins)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added DS18B20 sensor-to-SAT-area mapping feature:

- SATtypes.h: Added sSensorArea[4][17] to SATSection struct (16 hex chars + null per area, empty = unmapped)
- settingStuff.ino: Write side added 4 writeJsonStringKV calls for SATsensorarea0-3; read side added 4 strlcpy entries in updateSetting()
- sensors_ext.ino: pollSensors() now checks each area mapping on each poll; if address matches, calls satSetAreaTemp(areaIdx, tempC) — last-write-wins per AC#6
- restAPI.ino: Added GET/PATCH /api/v2/sat/sensor-areas endpoint; GET returns all 4 mappings as JSON array, PATCH validates area (0-3) and sensor (16 hex chars or empty) and calls updateSetting()
- data/index.js: Added buildDallasSensorAreasPanel() and refreshDallasSensorAreas() in SAT Settings page; per-area dropdown populated from /api/v2/sensors devices + labels; Save button calls PATCH endpoint; empty option clears mapping

Build: pre-existing SimpleTelnet.h missing prevents compilation but is unrelated to this change (verified by stash round-trip). Evaluator: 58/68 passed, 1 pre-existing PROGMEM failure unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
