---
id: TASK-935
title: >-
  feat(2.0.0): expose advanced OT-Direct + SAT settings over REST (v2 Settings
  Phase 2)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-25 19:15'
updated_date: '2026-06-25 19:20'
labels: []
dependencies: []
ordinal: 149000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2 of TASK-933. ~50 persisted OT-Direct PID/curve/bypass/vent + SAT DHW/pressure/zones/sensor-area/boiler-model keys are written to LittleFS but NOT exposed over GET /api/v2/settings nor accepted by POST (not in knownSettings[]). The v2 Settings page (TASK-933) shows the mockup's full OTD/SAT cards, but those fields cannot be wired until firmware exposes them. Expose the user-facing advanced keys: add GET emit (correct type/range), POST whitelist entry, and field-write dispatch for each; then add v2.js SET_META labels/hints + ENUM_OPTS where needed. Scope to the keys the mockup's cards actually surface; skip internal bookkeeping keys.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each targeted advanced OTD/SAT key is emitted by GET /api/v2/settings with the correct type and min/max
- [ ] #2 Each targeted key is in the POST whitelist and writes through to the settings struct + persists
- [ ] #3 v2.js SET_META gives each newly-exposed key a friendly label/hint/category; enums added where the value is an ordinal code
- [ ] #4 Firmware builds green (esp32 target) and evaluator stays green
- [ ] #5 Round-trip verified live: read a value, POST a change, confirm it persists across a reboot on the real device
<!-- AC:END -->
