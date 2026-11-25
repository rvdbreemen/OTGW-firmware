---
id: TASK-3044
title: >-
  Docs: the setup WiFi network name is wrong in the recovery guide and manual
  ch03, plus the owner first-run gaps (README steps 1-4, IP, /update)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T05:21:00Z'
updated_date: '2026-11-25T05:30:00Z'
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
- [x] #1 Setup network name: WIFI_RECOVERY_TRIPLE_RESET.md and ch03 (EN, and NL where the sentence exists) match networkStuff.ino:321-332 and say once that the fallback network (networkStuff.ino:503-513) is different. Evidence: grep of both source lines quoted in the task notes at fix time, and a grep over README, docs/guides and docs/manuals for OTGW-XXXXXX / OTGW-21B4F8 showing only fallback uses remain
- [ ] #2 README Quick Start steps 1 to 4 and the Which firmware section cover review items 2 to 6 (IP from the router list, first USB flash help, MQTT broker in one line plus link, /update address, how to tell the module); no new claim about what the web UI or OLED shows without checking it
- [x] #3 docs/MANUAL.md regenerated with python scripts/build_manual.py and python scripts/run_tests.py shows test_manual_generated passing; output quoted in the final summary
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-docs (95), AC1 evidence. Code verified at dev 81f1e5dc0: networkStuff.ino:321-332 builds thisAP = hostname + "-" + full eFuse MAC with colons (snprintf %02X:%02X:%02X:%02X:%02X:%02X); the only portal start is networkStuff.ino:442 manageWiFi.startConfigPortal(thisAP), so Reset WiFi, the triple reset and a boot without credentials all use that name; no password parameter, so the setup network is open. Default hostname OTGW2 (OTGW-firmware.h:136), e.g. OTGW2-A4:CF:12:21:B4:F8. networkStuff.ino:503-513 is the separate fallback AP: SSID OTGW-%02X%02X%02X (last 3 MAC bytes), password otgw1234, AP+STA so the STA keeps retrying.

Fixed: WIFI_RECOVERY_TRIPLE_RESET.md (setup name in steps 1 and 6, a 'Two different networks' table, the stale 'on prerelease builds, the AP fallback' clause removed: the fallback is in all builds since TASK-2931), manual ch02 EN/h02 NL (AP name format, step text 'otgw-XXXXXX', fallback name), ch03 EN/h03 NL (Reset WiFi sentence, the old 'derived from the chip ID' wrong), openapi.yaml apfallback description (no longer 'pre-release only', matches restAPI.ino:3337). README line 45 was already right. Grep after the fix over README, docs/guides, docs/manuals for OTGW-XXXXXX / OTGW-21B4F8 / OTGW-<MAC>: only fallback uses remain (ch02:253, ch06:45 EN and h06:57 NL, ch07:162, h02:287 and the new table row, all about the fallback network). MANUAL.md regenerated; python scripts/run_tests.py test_manual_generated: PASS 1.5s (4 tests). Not verified: the web UI or the OLED showing this name (no new claim made).

AC2 (README Quick Start steps 1 to 4 and the Which firmware section) is NOT done: per the coordinator it waits until he has cleared it with Robert (R1 landing); separate commit when cleared.
<!-- SECTION:NOTES:END -->
