---
id: TASK-3146
title: >-
  FS OTA: a refused filesystem upload leaves LittleFS unmounted until a reboot
  (ADR-298 guards decide too late)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T22:46:00Z'
labels:
  - bug
  - ota
milestone: 2.0.0
dependencies: []
ordinal: 1191000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .76 beta.482 (old table): POST /update?cmd=100&size=720896 and the same without size= are refused correctly, but only after the writer pause, the runtime-file copy, the RTC snapshot and LittleFS.end(); the completion handler remounts only when !LittleFSmounted and the start keeps it true, so version.hash, / and /settings.ini fail until /ReBoot. Fix: decide the three refusals first (commit 675520b10), order test red on the old code. Links TASK-2980, ADR-298.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A refused filesystem upload (no size=, too large, other table) touches nothing: no writer pause, no copy, no RTC snapshot, no LittleFS.end()
- [ ] #2 Host order test red on the old code and green on the fix; the existing OTA/keep/RTC harness tests follow the new contract
- [ ] #3 Bench-tester repeats both POSTs on the old-table unit: LittleFS stays mounted (version.hash, /, /settings.ini answer)
<!-- AC:END -->
