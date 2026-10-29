---
id: TASK-1406
title: >-
  PIC settings: reset_cause is a raw 0xFF byte after a PIC flash,
  /api/v2/pic/settings emits invalid UTF-8 JSON
status: To Do
assignee: []
created_date: '2026-10-26T06:05:00Z'
updated_date: '2026-10-28T21:09:00Z'
labels:
  - pic
  - bug
milestone: 2.0.0
dependencies: []
ordinal: 544000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench 2026-10-26 (Classic + S3 Mini .63, alpha.508, after a same-version gateway.hex 6.8 re-flash, TASK-1255): the PR= table showed Reset cause as the replacement character; GET /api/v2/pic/settings returned "reset_cause":"\xFF" (one raw byte), which is not valid UTF-8, so a strict JSON parser rejects the whole reply. Before the flash it read 'External reset'. The PR=Q answer after a PIC reprogram is stored and emitted without validation. Also seen: the post-flash settings poll ran 12 GETs, above PICSET_MAX_ITER=9 (check whether a field never arrives). Evidence: scratchpad task1255/ (screenshots, network list, timeline).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A non-printable or non-ASCII PR=Q reply is stored as unknown (or escaped), and /api/v2/pic/settings stays valid UTF-8 JSON (host test with a 0xFF answer, red on HEAD)
- [ ] #2 The v2 PR= table shows a readable value for that case
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28: 1.x sibling TASK-1430 (GH #688, field report on 1.7.8-beta.5) stores a non-printable PR= answer as "0x" + uppercase hex of every byte (0xFF -> "0xFF"), printable answers unchanged, via header-only copyPicSettingValue() (picSettingValue.h). Port the same representation so both lines publish the same value.
<!-- SECTION:NOTES:END -->
