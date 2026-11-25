---
id: TASK-3044
title: >-
  Docs: the setup WiFi network name is wrong in the recovery guide and manual
  ch03, plus the owner first-run gaps (README steps 1-4, IP, /update)
status: To Do
assignee:
  - '@otgw-docs'
created_date: '2026-11-25T05:21:00Z'
labels:
  - docs
  - ux
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1150000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw-ux-designer review docs/reviews/2026-11-25_ux-owner-first-run-text/README.md items 1 to 6, 2026-11-25. MUST FIX first: the setup network is <hostname>-<MAC with colons>, default hostname OTGW2 (e.g. OTGW2-A4:CF:12:21:B4:F8), built at networkStuff.ino:321-332 with one portal call site (:442). docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md:36-37 and manuals/en/ch03-web-interface.md:98 name it wrongly (OTGW-XXXXXX / OTGW-21B4F8). OTGW-XXXXXX + password otgw1234 is the separate fallback AP (networkStuff.ino:503-513). Then items 2 to 6: how to find the unit IP, first USB flash help (port, driver, no board found), MQTT step wording and link, the /update address, how to tell which module you have.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Setup network name: WIFI_RECOVERY_TRIPLE_RESET.md and ch03 (EN, and NL where the sentence exists) match networkStuff.ino:321-332 and say once that the fallback network (networkStuff.ino:503-513) is different. Evidence: grep of both source lines quoted in the task notes at fix time, and a grep over README, docs/guides and docs/manuals for OTGW-XXXXXX / OTGW-21B4F8 showing only fallback uses remain
- [ ] #2 README Quick Start steps 1 to 4 and the Which firmware section cover review items 2 to 6 (IP from the router list, first USB flash help, MQTT broker in one line plus link, /update address, how to tell the module); no new claim about what the web UI or OLED shows without checking it
- [ ] #3 docs/MANUAL.md regenerated with python scripts/build_manual.py and python scripts/run_tests.py shows test_manual_generated passing; output quoted in the final summary
<!-- AC:END -->
