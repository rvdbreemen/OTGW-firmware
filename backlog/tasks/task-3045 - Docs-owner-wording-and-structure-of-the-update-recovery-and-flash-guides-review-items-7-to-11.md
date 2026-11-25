---
id: TASK-3045
title: >-
  Docs: owner wording and structure of the update, recovery and flash guides
  (review items 7 to 11)
status: To Do
assignee:
  - '@otgw-docs'
created_date: '2026-11-25T05:23:00Z'
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
- [ ] #1 Owner guide for the WiFi update exists (max one page: migrate once, update from /update, what to do when it fails, heating warning first); engineering reference keeps its content; every link to WIFI_FIRMWARE_UPDATE.md in README, manuals and guides still resolves
- [ ] #2 WIFI_RECOVERY_TRIPLE_RESET.md starts with the numbered recovery steps and the power-dropout warning; FLASH_GUIDE.md says in its title that it is not for 2.0.0; the jargon list of the review is explained or replaced in README and the guides
- [ ] #3 docs/MANUAL.md regenerated with python scripts/build_manual.py and python scripts/run_tests.py shows test_manual_generated passing; the grep of both source of truth lines (networkStuff.ino:321-332 and :503-513) is repeated at fix time for any network name that appears
<!-- AC:END -->
