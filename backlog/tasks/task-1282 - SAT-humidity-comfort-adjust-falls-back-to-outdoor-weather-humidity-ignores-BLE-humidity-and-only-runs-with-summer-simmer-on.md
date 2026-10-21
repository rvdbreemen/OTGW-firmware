---
id: TASK-1282
title: >-
  SAT humidity: comfort adjust falls back to outdoor weather humidity, ignores
  BLE humidity, and only runs with summer simmer on
status: In Progress
assignee:
  - '@claude'
created_date: 2026-10-21 09:32
updated_date: '2026-10-21T20:10:00Z'
labels:
  - sat
  - humidity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 411000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-21 (#alpha-testing). Comfort / thermal-comfort fall back to outdoor weather humidity when no indoor value is pushed: wrong quantity, and 0 % with OWM. BLE sensor humidity (sat/ble_humidity) is not used as indoor humidity. Comfort adjust only runs when summer simmer is also enabled (SATcontrol.ino ~2942), which looks unintended. Its 30-min staleness is hardcoded instead of using humidity_timeout_s. Suggestion: indoor sources only (MQTT/REST/BLE), no weather fallback, and show the active humidity source in the UI. Changes SAT control behaviour (comfort adjust changes the target), so the plan needs Sergeant D's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's OK on the plan recorded
- [ ] #2 Indoor humidity comes only from MQTT/REST/BLE; no weather fallback (host test red on the old code)
- [ ] #3 Comfort adjust no longer depends on summer simmer and uses humidity_timeout_s (host test)
- [ ] #4 The UI shows the active humidity source
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Code confirmed (SATcontrol.ino): ~4731 copies weather humidity into state.sat.fHumidity when no direct value is valid (outdoor quantity; 0 % with the removed OWM); BLE humidity (state.sat.fBleHumidity, SATble.ino ~892) never feeds fHumidity; satUpdateComfort ~3147 returns unless bSummerSimmer is also on; the 30 min staleness is hardcoded ~3152 while settings.sat.iHumidityTimeoutS exists (used for thermal comfort ~1265). thermo-nova uses only an indoor humidity sensor, no weather fallback (climate.py current_humidity); comfort adjust is our own feature (Task #28/#47).
Plan: 1) indoor sources only: MQTT/REST push (existing), and BLE humidity from the BLE sensor that provides the room temperature (or any fresh BLE humidity when BLE is the indoor source); drop the weather fallback. Priority: pushed MQTT/REST value if fresh, else BLE. 2) comfort adjust no longer requires summer simmer. 3) staleness of humidity for comfort uses iHumidityTimeoutS (same as thermal comfort). 4) status/MQTT/UI show the active humidity source (none/mqtt/ble). Verification: host test of the source selection and comfort gate (red on HEAD), bench with pushed humidity / BLE sim / weather on.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-21 plan posted to Sergeant D (#alpha-testing msg 1562558755898523739); open points: comfort adjust outside summer, pushed value vs BLE priority. Waiting for his OK; cycle continues.
<!-- SECTION:NOTES:END -->
