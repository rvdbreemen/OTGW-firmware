---
id: TASK-782
title: Add Publish on change checkbox to MQTT settings UI
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 14:51'
updated_date: '2026-05-31 14:52'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the raw MQTT Publish Interval number field with a Publish on change checkbox + conditional Interval field. Checkbox off = iInterval 0 (publish every frame). Checkbox on = iInterval visible, default 60s (change-detection active). UI-only change; backend iInterval semantics unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Checkbox renders above Interval field in MQTT settings
- [x] #2 Checkbox off: interval field hidden, iInterval saved as 0
- [x] #3 Checkbox on: interval field visible, default 60s
- [x] #4 Build passes, evaluator green
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
UI-only change in data/index.js. Injects a synthetic 'Publish on change' checkbox (id=mqttpublishonchange) immediately before the mqttinterval row. Checkbox state derives from iInterval!=0. Toggle drives mqttinterval to 0 or 60 and marks it input-changed for saveSettings(). Interval row hidden via display:none when value is 0. Backend unchanged. Build and evaluator green (100%). Pushed to origin/dev.
<!-- SECTION:FINAL_SUMMARY:END -->
