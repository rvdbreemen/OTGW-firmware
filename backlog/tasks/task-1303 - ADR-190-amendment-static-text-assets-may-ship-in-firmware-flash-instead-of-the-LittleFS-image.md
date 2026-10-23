---
id: TASK-1303
title: >-
  ADR-190 amendment: static text assets may ship in firmware flash instead of
  the LittleFS image
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T17:35:00Z'
updated_date: '2026-10-23T10:42:00Z'
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
- [x] #1 ADR drafted via adr-kit as Proposed, referencing TASK-1294 evidence (image size before/after) and passing lint
- [x] #2 ADR-190 and ADR-139 related lists point at it; CLAUDE.md asset rules unchanged (still no gzip)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-194 (Proposed, guideline-level per ADR-080) records the TASK-1294 decision: a static text asset users never upload may be compiled into the firmware and served from PROGMEM (serveProgmemAsset, ETag <githash>-<path>, Cache-Control no-cache, 304) or read through a PROGMEM reader; a built-in default is allowed when a LittleFS override wins (replay fixture). Everything else stays in the minified LittleFS image, no gzip (CLAUDE.md asset rules unchanged). It narrows ADR-190/ADR-139 without reversing them. Evidence: AC1 drafted with adr-kit (adr new --profile canonical), cites TASK-1294's margin numbers (664 -> 628 KiB build line, margin 8 -> 44 KiB) and the measured asset sizes (help JSON 18,398 B, fixture 12,716 B), quality 0.92 (A), directory lint: ADR-194 ADVISORY only (one open question for the maintainer), no new FAIL (56 FAIL pre-existing at HEAD); AC2 ADR-190 and ADR-139 related lists point at ADR-194 (adr relate), governance test 15 OK. Open for the maintainer: accept ADR-194 and answer its open question (is the three-part rule the standing criterion?). Side effect: the README table shows 2026-10-23 for ADR-139/190 because the relate entries add a status-history line.
<!-- SECTION:FINAL_SUMMARY:END -->
