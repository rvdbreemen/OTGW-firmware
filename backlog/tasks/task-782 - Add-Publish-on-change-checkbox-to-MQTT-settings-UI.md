---
id: TASK-782
title: Add Publish on change checkbox to MQTT settings UI
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 14:51'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the raw MQTT Publish Interval number field with a Publish on change checkbox + conditional Interval field. Checkbox off = iInterval 0 (publish every frame). Checkbox on = iInterval visible, default 60s (change-detection active). UI-only change; backend iInterval semantics unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Checkbox renders above Interval field in MQTT settings
- [ ] #2 Checkbox off: interval field hidden, iInterval saved as 0
- [ ] #3 Checkbox on: interval field visible, default 60s
- [ ] #4 Build passes, evaluator green
<!-- AC:END -->
