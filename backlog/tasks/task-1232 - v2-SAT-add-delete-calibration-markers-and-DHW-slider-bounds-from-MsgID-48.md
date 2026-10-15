---
id: TASK-1232
title: 'v2 SAT: add/delete calibration markers and DHW slider bounds from MsgID 48'
status: To Do
assignee: []
created_date: '2026-10-15 05:10'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: low
ordinal: 370000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md rows 57, 58: v2 lists markers but cannot add or delete them (sat/markers POST/DELETE); the DHW slider has fixed 40-65 bounds instead of the boiler's MsgID 48 range. Maintainer decided 2026-10-15: keep v2's persistent dhw_setpoint semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 can add a marker by clicking the curve and delete one
- [ ] #2 The DHW slider bounds come from MsgID 48 when the boiler reports them, else 40-65
- [ ] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->
