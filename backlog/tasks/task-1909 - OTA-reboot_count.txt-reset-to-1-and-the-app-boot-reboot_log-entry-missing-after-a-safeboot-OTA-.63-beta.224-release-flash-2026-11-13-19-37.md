---
id: TASK-1909
title: >-
  OTA: reboot_count.txt reset to 1 and the app-boot reboot_log entry missing
  after a safeboot OTA (.63, beta.224 release flash 2026-11-13 19:37)
status: To Do
assignee: []
created_date: '2026-11-13T18:53:00Z'
labels:
  - bug
  - ota
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 975000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09 bench: OTA app+fs of 2.0.0-beta.224+b769b8f on .63 (Classic + S3 Mini Pro, PIC 6.8). bootcount went 35 -> 1 instead of 37; /reboot_count.txt holds 1 (on the keep list, OTGW-ModUpdateServer-esp32.h:633, 16 B cap); reboot_log.txt kept old lines but has only the 19:37:32 [OTA] Rebooting entry, the app-boot entry every earlier flash wrote is missing. Same src flashed at 16:22 kept the counter (33 -> 35). Settings intact (150 keys, none changed). Not established: whether sat_tune.json, sat_markers.json, dallas_labels.ini existed before. No telnet capture of this flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce with a telnet capture across an OTA of app+fs on a Classic bench unit (09), or a host test of the keep/restore path that shows the counter lost
- [ ] #2 Root cause named with file:line
- [ ] #3 Fix: reboot_count and reboot_log survive an OTA; red on the old code, green on the fix
<!-- AC:END -->
