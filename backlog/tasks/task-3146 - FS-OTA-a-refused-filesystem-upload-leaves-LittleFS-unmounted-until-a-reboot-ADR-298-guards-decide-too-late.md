---
id: TASK-3146
title: >-
  FS OTA: a refused filesystem upload leaves LittleFS unmounted until a reboot
  (ADR-298 guards decide too late)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T22:46:00Z'
updated_date: '2026-11-28T19:20:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-11-28 12:06 on .76 (beta.496+313c4ba, githash 313c4ba contains 675520b10): three refused FS uploads (POST /update?cmd=100 without size=, with size=900000, with size=4096, a 4096-byte dummy body; nothing valid sent). Each answered 'Flash error: ...' (no size=; image too large; does not match this unit's partition layout) and telnet logged '[OTA] Refused before touching LittleFS' right after '[OTA] Target: filesystem (786432 bytes)'; no pause, copy, RTC snapshot, restore or unmount line between the refusals. After each POST /version.hash, / and /settings.ini answered 200 (before: 200/200/200). /api/v2/settings byte-identical before and after (cmp), bootcount 1253 unchanged, fsupdateincomplete false. Evidence: OTGW-field-data/bench-61/2026-11-28_sat313c4ba/ota3146/.

AC3 status 2026-11-28 12:10, NOT ticked and not waived (a waiver is Robert's decision, on his Monday list). Verified: new-table unit .76 (beta.496+313c4ba), refusal path with three POSTs. Not verified: an old-table unit, a full-size image sized for another table. AC1 and AC2 stay with dev.
<!-- SECTION:NOTES:END -->
