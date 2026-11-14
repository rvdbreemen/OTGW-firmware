---
id: TASK-1919
title: >-
  An app image that the bootloader marked ABORTED keeps running but never
  confirms itself, so the next reset of any kind rolls back to safeboot
status: To Do
assignee: []
created_date: '2026-11-14T04:02:00Z'
labels:
  - bug
  - ota
  - safeboot
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 984000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found with the TASK-1774 RTC probe (otgw-bench-tester 09, .63, 2026-11-14 04:50-04:57, 09 scratchpad b1774\). A freshly flashed image (img=PENDING_VERIFY) that panics did not roll back on the first reset: ota_0 booted again and reported img=ABORTED reset=4; only the second panic rolled back to safeboot (/info app_rolled_back=true). safebootMarkAppValid() (OTGW-ModUpdateServer-esp32.h:154-161) confirms only PENDING_VERIFY, so the app running as ABORTED never confirms itself, and any later reset (planned restart, settings change, FS OTA, crash) rolls it back to safeboot. Fits beta.217 on .63 (2026-11-12): panic during an FS write, unit came back on main, rollback only after the 2nd FS upload (TASK-1774 notes). Reading (not verified): local framework-espidf bootloader_utility.c:396-405 marks PENDING_VERIFY as ABORTED at the next boot; :407-416 falls back to factory only when both otadata entries are invalid, else :440-443 boots the active valid entry, ota_0 with one OTA slot. That implies a second, still valid otadata entry; who left it is not shown, and the pinned bootloader is a precompiled binary. Pending proof: esptool read_flash of otadata (0xE000, 2 x 4 KB) on .63 in three states (fresh probe flash, after the 1st panic, after the rollback), by 09. Any change to when an image confirms itself changes ADR-191 rollback behaviour: maintainer decision, not decided here.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 otadata dumps in the three states show the mechanism (which entry is active, its state and seq) or refute this reading
- [ ] #2 Robert decides how an ABORTED-running image is handled (options with consequences, put by the coordinator)
- [ ] #3 If a change is decided: old-vs-fix bench on .63 (panic, ABORTED boot, planned restart: old rolls back, fix per the decision)
<!-- AC:END -->
