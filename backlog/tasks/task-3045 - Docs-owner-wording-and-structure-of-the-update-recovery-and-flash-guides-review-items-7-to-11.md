---
id: TASK-3045
title: >-
  Docs: owner wording and structure of the update, recovery and flash guides
  (review items 7 to 11)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T05:23:00Z'
updated_date: '2026-11-25T05:34:00Z'
labels:
  - docs
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1151000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw-ux-designer review docs/reviews/2026-11-25_ux-owner-first-run-text/README.md items 7 to 11, 2026-11-25. Explain or replace jargon (filesystem, flash layout, USB migration, safeboot, captive portal, access point, mDNS, PIC, ESP); one vocabulary (gateway, unit, web UI, WiFi); split WIFI_FIRMWARE_UPDATE.md into a one-page owner guide plus the engineering reference, with the heating-interruption warning (1 to 2 minutes on the boiler's own settings) at the top of the update section; WIFI_RECOVERY_TRIPLE_RESET.md steps first, mechanism last; FLASH_GUIDE.md retitled or moved as 1.x ESP8266 only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Owner guide for the WiFi update exists (max one page: migrate once, update from /update, what to do when it fails, heating warning first); engineering reference keeps its content; every link to WIFI_FIRMWARE_UPDATE.md in README, manuals and guides still resolves
- [ ] #2 WIFI_RECOVERY_TRIPLE_RESET.md starts with the numbered recovery steps and the power-dropout warning; FLASH_GUIDE.md says in its title that it is not for 2.0.0; the jargon list of the review is explained or replaced in README and the guides
- [x] #3 docs/MANUAL.md regenerated with python scripts/build_manual.py and python scripts/run_tests.py shows test_manual_generated passing; the grep of both source of truth lines (networkStuff.ino:321-332 and :503-513) is repeated at fix time for any network name that appears
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-docs (95). AC1: WIFI_FIRMWARE_UPDATE.md is now the owner guide (55 lines: heating warning first, what you need, one-time USB migration, update from /update, what to do when it fails, a short word list); the old content moved verbatim to the new WIFI_FIRMWARE_UPDATE_REFERENCE.md (header note added, nothing removed). The owner guide keeps the old file name on purpose: README (lines 39 and 310) and 22 release-notes files, among them published releases, link to docs/guides/WIFI_FIRMWARE_UPDATE.md; no link uses an anchor (git grep), so every existing link still resolves. tests/test_fs_update_keep.py (reads two sections of the guide) and a comment in tests/webui/update-orchestrator.test.mjs now point at the reference file.

AC2 partly: WIFI_RECOVERY_TRIPLE_RESET.md now starts with the numbered recovery steps and the power-dropout warning, mechanism last; FLASH_GUIDE.md is titled 'Flashing the 1.x ESP8266 (not for 2.0.0)'; jargon in the guides explained or replaced (filesystem -> web files, flash layout explained, safeboot = updater, captive portal -> setup page, access point explained once, WiFiManager and ADR numbers only in the mechanism part; mDNS only as 'use the address, not otgw2.local'). NOT done and not ticked: the same jargon in README.md (filesystem, flash layout, USB migration, safeboot, captive portal, access point in the disclaimer, Quick Start and highlights): README is held until the coordinator has Robert's clearance (R1). The 'one vocabulary' item (gateway / unit / web UI / WiFi) is applied in the three guides only; ch02/ch06 manuals still mix unit/device.

AC3: python scripts/build_manual.py wrote docs/MANUAL.md (no diff: no chapter changed); python scripts/run_tests.py test_manual_generated test_fs_update_keep: PASS test_manual_generated (4 tests), PASS test_fs_update_keep (38 tests). Source lines re-read at origin/dev 52c7b91ca: networkStuff.ino:322-332 builds thisAP = hostname + '-' + the six MAC bytes as %02X with colons, :442 is the only startConfigPortal(thisAP) call, :513 WiFi.softAP(apSSID, "otgw1234") is the fallback; the recovery guide's network table matches them. No new claim about what the web UI or OLED shows. TASK-3045 stays In Progress for the README part of AC2.
<!-- SECTION:NOTES:END -->
