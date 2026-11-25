---
id: TASK-3044
title: >-
  Docs: the setup WiFi network name is wrong in the recovery guide and manual
  ch03, plus the owner first-run gaps (README steps 1-4, IP, /update)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-25T05:21:00Z'
updated_date: '2026-11-25T17:22:00Z'
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
- [x] #2 README Quick Start steps 1 to 4 and the Which firmware section cover review items 2 to 6 (IP from the router list, first USB flash help, MQTT broker in one line plus link, /update address, how to tell the module); no new claim about what the web UI or OLED shows without checking it
- [x] #3 docs/MANUAL.md regenerated with python scripts/build_manual.py and python scripts/run_tests.py shows test_manual_generated passing; output quoted in the final summary
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-docs (95), AC1 evidence. Code verified at dev 81f1e5dc0: networkStuff.ino:321-332 builds thisAP = hostname + "-" + full eFuse MAC with colons (snprintf %02X:%02X:%02X:%02X:%02X:%02X); the only portal start is networkStuff.ino:442 manageWiFi.startConfigPortal(thisAP), so Reset WiFi, the triple reset and a boot without credentials all use that name; no password parameter, so the setup network is open. Default hostname OTGW2 (OTGW-firmware.h:136), e.g. OTGW2-A4:CF:12:21:B4:F8. networkStuff.ino:503-513 is the separate fallback AP: SSID OTGW-%02X%02X%02X (last 3 MAC bytes), password otgw1234, AP+STA so the STA keeps retrying.

Fixed: WIFI_RECOVERY_TRIPLE_RESET.md (setup name in steps 1 and 6, a 'Two different networks' table, the stale 'on prerelease builds, the AP fallback' clause removed: the fallback is in all builds since TASK-2931), manual ch02 EN/h02 NL (AP name format, step text 'otgw-XXXXXX', fallback name), ch03 EN/h03 NL (Reset WiFi sentence, the old 'derived from the chip ID' wrong), openapi.yaml apfallback description (no longer 'pre-release only', matches restAPI.ino:3337). README line 45 was already right. Grep after the fix over README, docs/guides, docs/manuals for OTGW-XXXXXX / OTGW-21B4F8 / OTGW-<MAC>: only fallback uses remain (ch02:253, ch06:45 EN and h06:57 NL, ch07:162, h02:287 and the new table row, all about the fallback network). MANUAL.md regenerated; python scripts/run_tests.py test_manual_generated: PASS 1.5s (4 tests). Not verified: the web UI or the OLED showing this name (no new claim made).

AC2 (README Quick Start steps 1 to 4 and the Which firmware section) is NOT done: per the coordinator it waits until he has cleared it with Robert (R1 landing); separate commit when cleared.

Evidence for 'the AP fallback is in all builds since TASK-2931' (file:line at dev 81f1e5dc0): networkStuff.ino:496-497 (comment 'AP fallback ... TASK-2931 (Robert, DM 2026-11-19): part of prerelease AND stable builds') above startAPFallback() at :499, with no prerelease guard inside it (grep for _VERSION_PRERELEASE / IS_PRERELEASE in networkStuff.ino: no hit); the decision to enter it is networkStuff.ino:653 ('in prerelease and stable builds alike. After 2 failed ...'); restAPI.ino:3337 and :4099 emit apfallback in both (comment 'TASK-2931: AP fallback in prerelease and stable builds'). Not run on a stable build image: read from the code only.

AC2 done 2026-11-25 (Robert cleared README steps 1-4 via the coordinator; wording from otgw-ux-designer, ux-readme-quickstart-steps-1-4.md). Checked: /update route OTGW-ModUpdateServer-esp32.h:460; 'Download a settings backup' link on the update page (manual ch03:240), so the unverified 'Settings > Download settings / settings.ini' clause was dropped; USB: ch02 says built-in USB, normally no driver, flash_otgw --port (ch02:165-180), so the CP210x/CH340 and 'charge-only cable' clauses were dropped (CP210x/CH340 is the 1.x ESP8266 guide); update warning: WIFI_FIRMWARE_UPDATE.md:5 says the gateway does nothing for the heating for 1 to 2 minutes, boiler-own-settings applies only to boards without PIC, so README says only 'does nothing for your heating'; old layout: ch02:179 and the guide say early 2.0.0 alpha; step 3 names only the router (hostname OTGW2 default, OTGW-firmware.h:136; whether the router shows it is not verified); anchors setting-up-mqtt-with-home-assistant (README:192) and ch02 flashing-the-firmware (ch02:148) exist. Also fixed the stale link label 'beta.418 release notes'. Not verified: how a router displays the gateway; no OLED/UI claim made.

Release commit for v2.0.0-beta.432 (notes, CHANGELOG rollover, README beta link) carries this task: its three ACs are met (AC1/AC3 earlier, AC2 in a3ad52d08). The beta.432 notes name the README Quick Start change under TASK-3044.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: setup network name fixed in WIFI_RECOVERY_TRIPLE_RESET.md, manual ch02/ch03 EN+NL, openapi apfallback (grep shows only fallback uses of OTGW-XXXXXX remain). AC2: README Quick Start steps 1, 3, 4 in a3ad52d08 (checked per clause against code and manual; unverifiable clauses dropped, see notes). AC3: MANUAL.md regenerated, test_manual_generated passed. Not verified: whether a router displays the hostname OTGW2; no OLED or web UI claim made; no first-time owner has read the new text.
<!-- SECTION:FINAL_SUMMARY:END -->
