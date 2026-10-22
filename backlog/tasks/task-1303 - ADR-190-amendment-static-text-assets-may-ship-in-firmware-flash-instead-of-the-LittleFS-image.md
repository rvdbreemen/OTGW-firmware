---
id: TASK-1303
title: >-
  ADR-190 amendment: static text assets may ship in firmware flash instead of
  the LittleFS image
status: To Do
assignee: []
created_date: '2026-10-22T17:35:00Z'
labels:
  - adr
  - docs
  - littlefs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 426000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1294 (maintainer decision 2026-10-22) moves settings-help.json and the built-in otgw_simulation.log fixture out of the LittleFS image into PROGMEM, served/used by the firmware, to restore headroom in the 768 KiB partition. ADR-190 states the image keeps every file in data/ (minified, no gzip); ADR-139 keeps 'no gzip'. Both remain in force; this is a narrowing (two text assets served from firmware), not a reversal. adr-kit (plugin and CLI) was not reachable in the session that made the change, so the amendment is pending. Write it via adr-kit: Proposed, context = the 8 KiB margin after TASK-1281 and the levers the maintainer rejected (PIC hex files, fonts, gzip), decision = text assets without a user-upload role may live in firmware flash with a firmware-hash ETag; consequences = help text changes need a firmware update, the replay fixture is always available.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR drafted via adr-kit as Proposed, referencing TASK-1294 evidence (image size before/after) and passing lint
- [ ] #2 ADR-190 and ADR-139 related lists point at it; CLAUDE.md asset rules unchanged (still no gzip)
<!-- AC:END -->
