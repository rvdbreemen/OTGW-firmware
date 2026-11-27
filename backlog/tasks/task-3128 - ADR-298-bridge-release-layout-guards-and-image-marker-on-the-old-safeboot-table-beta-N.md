---
id: TASK-3128
title: >-
  ADR-298 bridge release: layout guards and image marker on the old safeboot
  table (beta N)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-27T18:29:00Z'
updated_date: '2026-11-27T19:53:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1180000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First of the two ADR-298 releases (Robert, Open Question 3: bridge first, then the table change). Carries the guards on the OLD table (app0 0x230000, LittleFS 0x330000/768K) and marks its own image otgw-layout=safeboot. Existing code: TASK-2980 lane/d3-trim commits b86614dec, c93f9fa4d, 27c6c4930, bd58685cb and f384db4cb (the guard commits; 76ad1afaf is the table change and belongs to the next task). No branch points at these commits any more (git for-each-ref --contains f384db4cb is empty, 2026-11-27 19:5x): anchor them on a branch first. Rebase onto origin/dev and leave the table itself unchanged in this release. Source: docs/adr/ADR-298 Decision Contract (Must) and Verification.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 USB tools (flash_otgw.bat, flash_otgw.sh, flash_esp.py) tell the layout by the app0 size (0x230000 = safeboot, 0x240000 = safeboot2), refuse --update on a mismatch and point to --migrate; host test red on the code before (string 'safeboot' match), green after
- [ ] #2 The main firmware's filesystem update refuses, before any erase, an image whose size differs from the partition and an upload without size= in the query string; host test red/green
- [ ] #3 /api/v2/device/info reports partition_layout (legacy, safeboot, safeboot2, unknown) and partition_layout_expected; telnet boot warning when they differ; the /update page blocks the one-action update on a mismatch and points to --migrate
- [ ] #4 Every app image carries otgw-layout=<layout>; this release's image says safeboot. The /update page and scripts/ota_flash.py read the marker before sending and refuse an image whose marker differs from the unit's partition_layout; an image without a marker is not blocked; the page builds its search string at run time; host tests red/green, including a mutant image with the wrong marker
- [ ] #5 flash_esp.py writes a partition table only together with both the firmware and the filesystem image; host test red/green
- [ ] #6 tests/test_partition_mirrors.py: every mirror equals partitions_otgw_esp32.csv (still the old layout here); red on a mutant mirror
- [ ] #7 Bench (otgw-bench-tester) on .63 and .76: OTA with this zip through the browser and ota_flash.py works on the old table; a fake safeboot2 image is refused by /update and ota_flash.py with nothing changed; device/info shows partition_layout safeboot = expected; 60 min soak per unit; capture transcripts
- [ ] #8 python evaluate.py --quick no new FAIL; esp32-combo build; prerelease bump; app image under the 98 % gate of the OLD app0
<!-- AC:END -->
