---
id: TASK-3018
title: >-
  settingsHelp: satzoneheadroom says 'Unused with 4 zones or fewer' while
  satzonecount only allows 1-4
status: To Do
assignee: []
created_date: '2026-11-21T17:04:00Z'
labels:
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1134000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-webui while drafting TASK-3014 (2026-11-21). settingsHelp.h describes satzoneheadroom as unused with 4 zones or fewer, but satzonecount is limited to 1..4, so as written the headroom never applies. Either the help text is wrong (check where iZoneHeadroom is used in SATcontrol.ino) or the range is. Fix the text or the range, regenerate settingsHelpGz.h.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The code path that reads the zone headroom is cited (file:line) and the help text states when it applies
- [ ] #2 settingsHelpGz.h regenerated, test_progmem_assets_deflated green
<!-- AC:END -->
