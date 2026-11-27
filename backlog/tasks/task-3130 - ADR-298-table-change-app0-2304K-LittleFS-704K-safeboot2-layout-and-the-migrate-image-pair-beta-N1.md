---
id: TASK-3130
title: >-
  ADR-298 table change: app0 2304K, LittleFS 704K, safeboot2 layout and the
  --migrate image pair (beta N+1)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-27T18:39:00Z'
updated_date: '2026-11-27T19:53:00Z'
labels: []
milestone: 2.0.0
dependencies:
  - TASK-3128
priority: high
ordinal: 1181000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Second ADR-298 release, after the bridge (TASK-3128) is out. Existing code: TASK-2980 lane/d3-trim 76ad1afaf (table) on no branch; rebase onto dev after TASK-3128 lands. Source: docs/adr/ADR-298 Decision, Decision Contract and Verification. Docs C3 ride in the same commit (separate docs C3 task).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 partitions_otgw_esp32.csv: app0 0x100000 size 0x240000, spiffs 0x340000 size 0xB0000; nvs, otadata, safeboot, coredump unchanged; layout name safeboot2; every mirror (build.py, flash_otgw.bat/.sh, flash_esp.py, check_app_size.py, otgw-test.py, device-verify.sh, update.html APP_MAX/FS_SIZE, OTGW_PARTITION_LAYOUT) equal in the same commit; tests/test_partition_mirrors.py green, red on a mutant
- [ ] #2 Images carry otgw-layout=safeboot2; a unit on the bridge refuses this release over WiFi (browser and ota_flash.py) and points to --migrate
- [ ] #3 Migrate image pair built for the new table; flash_otgw --migrate accepts a safeboot unit as source; NVS not touched; settings saved over HTTP first and restored
- [ ] #4 Build log: app image under the 98 % gate of the 2304K app0; LittleFS budget OK against 704K
- [ ] #5 Bench (otgw-bench-tester) on .63 and .76 from the bridge release: OTA with this zip refused, nothing changed; --update with the new images on the old table refused; --migrate with settings.ini and dallas_labels.ini 0 differences, WiFi kept, duration measured; rollback with the old release's full merged image boots the old layout; ADR-233 RTC snapshot after a failed mount checked; 60 min soak per unit; capture transcripts
- [ ] #6 No rewrite of the partition table from the running firmware or safeboot (ADR-298 Must Not); no release while any tool detects the layout by the string safeboot only
- [ ] #7 Release notes and the #beta-testing notice (BlueFlame or otgw-docs) say: one --migrate over USB per 4MB unit, OTA alone is not enough, breaking change, rollback only with the old full merged image; Robert's wording requirement from ADR-298 Must quoted
<!-- AC:END -->
