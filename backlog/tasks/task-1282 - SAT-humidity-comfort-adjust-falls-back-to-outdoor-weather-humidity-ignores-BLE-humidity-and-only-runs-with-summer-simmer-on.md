---
id: TASK-1282
title: >-
  SAT humidity: comfort adjust falls back to outdoor weather humidity, ignores
  BLE humidity, and only runs with summer simmer on
status: To Do
assignee: []
created_date: '2026-10-21 09:32'
updated_date: '2026-10-21 09:33'
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
