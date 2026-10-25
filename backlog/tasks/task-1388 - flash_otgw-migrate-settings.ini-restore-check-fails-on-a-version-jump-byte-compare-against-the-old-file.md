---
id: TASK-1388
title: >-
  flash_otgw --migrate: settings.ini restore check fails on a version jump (byte
  compare against the old file)
status: To Do
assignee: []
created_date: '2026-10-25T11:36:00Z'
labels:
  - tooling
  - flash
milestone: 2.0.0
dependencies: []
ordinal: 527000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563871156329775135): migrate from alpha.404 to alpha.500 succeeded and his settings look fine, but the script printed '[ERROR] settings.ini on the unit differs from the saved copy'. flash_otgw.bat:496-499 downloads settings.ini after the restore + reboot and compares it byte for byte (fc /b) with the saved copy. The new firmware reads the old file and writes it back in its own format (new keys, defaults, numeric precision, migrated values), so a version jump always differs. Fix: compare per key: every key of the saved file must be present after the restore with the same value (numbers compared as numbers), new keys allowed; report the keys that differ. Same check in flash_otgw.sh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A migrate across a version jump with all settings kept reports OK (repro with an old settings.ini, red on HEAD)
- [ ] #2 A real loss (a saved key missing or changed) is still reported, naming the keys
- [ ] #3 flash_otgw.bat and flash_otgw.sh behave the same
<!-- AC:END -->
