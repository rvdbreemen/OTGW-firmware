---
id: TASK-278
title: 'Fix: v1.4.0-beta ESP8266 crash/reboot loop (Exception 2/3)'
status: Done
assignee: []
created_date: '2026-04-16 16:50'
updated_date: '2026-04-17 07:09'
labels:
  - bug
  - needs-info
  - esp8266
dependencies: []
references:
  - 'Discord #beta-testing, user crashevans, 2026-04-13 to 2026-04-16'
  - >-
    Serial logs: otgw-v1.4.0-log-20260416-121047.txt,
    otgw-v1.4.0-log-20260416-121121.txt
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ESP8266 enters crash/reboot loop after flashing v1.4.0-beta. Reporter crashevans sees Exception (2) and Exception (3) on boot. Free heap critically low (13248 bytes). Root cause likely Arduino Core 3.1.2 IP stack changes causing heap pressure. number3nl is actively debugging on the 1.4.0 branch. AutoDiscovery rework attempted but not yet resolved.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v1.4.0-beta boots without crash loop on ESP8266 with crashevans' hardware config
- [ ] #2 Free heap remains above safe threshold during normal operation
- [ ] #3 MQTT auto-config completes without Exception crashes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed by PROGMEM-as-RAM crash fix (commits a91220af + 413d8b00). Both Exception (3) and Exception (28) were caused by strstr/strncmp/strlen on PROGMEM flash pointers. Fix eliminated all standard C library calls on PROGMEM data in autodiscovery code.
<!-- SECTION:FINAL_SUMMARY:END -->
