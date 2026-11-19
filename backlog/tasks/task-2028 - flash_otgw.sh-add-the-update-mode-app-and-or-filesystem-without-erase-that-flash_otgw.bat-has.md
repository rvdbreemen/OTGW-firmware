---
id: TASK-2028
title: >-
  flash_otgw.sh: add the --update mode (app and/or filesystem without erase)
  that flash_otgw.bat has
status: To Do
assignee: []
created_date: '2026-11-18T14:02:00Z'
labels:
  - tools
milestone: 2.0.0
dependencies: []
ordinal: 1040000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-2027: the recovery in manual 7.12a (reset settings.ini over USB, keep WiFi) works on Windows only; flash_otgw.sh parses --port/--bin/--board/--baud/--migrate/--host/--password/--no-backup (:145-152) and has no --update. Linux/macOS users must factory-flash and lose WiFi.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 flash_otgw.sh --update [--app] [--fs] writes at the bat offsets (app0 0x100000, spiffs 0x330000) without erase, refuses on a non-safeboot layout like the bat
- [ ] #2 Host test or a real run on a unit shows WiFi kept
- [ ] #3 Manual 7.12a step 2 updated to drop the Windows-only note
<!-- AC:END -->
