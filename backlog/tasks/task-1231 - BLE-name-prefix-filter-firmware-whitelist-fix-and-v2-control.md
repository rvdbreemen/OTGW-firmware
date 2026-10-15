---
id: TASK-1231
title: 'BLE name-prefix filter: firmware whitelist fix and v2 control'
status: To Do
assignee: []
created_date: '2026-10-15 05:09'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: low
ordinal: 369000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md row 51: the BLE name-prefix and restrict-ingest keys are not in knownSettings (restAPI.ino:4200), so a POST returns 400 even from classic. Fix the firmware first, then add the v2 control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 POST of the BLE filter keys is accepted and the values are exposed in GET settings (host or bench transcript)
- [ ] #2 v2 has controls for the filter and the restrict-ingest toggle
- [ ] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->
