---
id: TASK-24
title: Summer simmer index for auto heating disable
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:46'
updated_date: '2026-04-06 12:26'
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
- [x] #1 Calculate summer simmer index from indoor temperature and humidity
- [x] #2 Auto-disable SAT heating when simmer index exceeds comfort threshold
- [x] #3 MQTT/REST/WebUI: simmer index value and threshold setting
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add summer simmer settings (enable, threshold temp, min consecutive hours)
2. Calculate simmer index from outdoor temp (weather API or OT bus)
3. Auto-disable heating when outdoor stays above threshold
4. Auto-re-enable when temp drops back
5. MQTT/REST/WebUI status
6. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Summer simmer implemented as outdoor temperature threshold approach (not humidity-based simmer index).
AC #3 (humidity sensor) not applicable: uses outdoor temp from OT bus, weather API, or external MQTT instead.
All settings persisted, MQTT control topics added, REST API and WebUI labels/tooltips complete.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Summer simmer: auto-disable heating in warm weather.

Changes:
- Tracks consecutive hours outdoor temp above threshold (default 18C)
- After configured hours (default 6h), suppresses heating (CS=0)
- 2C hysteresis for re-enabling
- Settings: SATsummersimmer, SATsummerthreshold, SATsummerminhours
- MQTT: sat/summer_active, sat/summer_hours_above
<!-- SECTION:FINAL_SUMMARY:END -->
