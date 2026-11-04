---
id: TASK-1464
title: >-
  Snowfall unit: discovery says mm, Open-Meteo request sets no unit (default may
  be cm)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:22:00Z'
updated_date: '2026-11-01T01:22:00Z'
labels:
  - sat
  - weather
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 624000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTTHaDiscovery.cpp:1369 announces mm; SATweather.ino:68-75 sets no snowfall unit. The Open-Meteo default unit for snowfall is unverified (the auditor wrote it from memory).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Open-Meteo documentation checked (link in notes); the request or the announced unit fixed so they match
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Open-Meteo reports snowfall in centimetres: docs https://open-meteo.com/en/docs ('cm (inch)', 'Snowfall amount of the preceding hour in centimeters') and the live API (api.open-meteo.com/v1/forecast?...&current=snowfall returns current_units.snowfall = "cm", 2026-11-01). The request keeps no unit parameter; the HA discovery row now announces HaUnit::cm (new enum value + string), docs/api/MQTT.md updated. Evidence: tests/test_weather_snowfall_unit.py new 3/3, old 2 FAIL; test_sat_energy_discovery and ha-discovery-golden still pass; evaluate 0 FAIL. Not verified on the bench: a HA instance showing the cm unit (needs the next build). Existing HA entities keep their stored unit until HA re-reads discovery.
<!-- SECTION:FINAL_SUMMARY:END -->
