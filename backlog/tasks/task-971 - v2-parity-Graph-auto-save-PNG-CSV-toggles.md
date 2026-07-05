---
id: TASK-971
title: 'v2 parity: Graph auto-save PNG/CSV toggles'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 23:11'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 183000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (1 hit in v2): Classic Graph has auto-save PNG + auto-save CSV toggles. v2 has manual export only. Low priority.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 Graph adds auto-save PNG toggle
- [x] #2 v2 Graph adds auto-save CSV toggle
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: Graph auto-save PNG/CSV toggles. Added Auto-PNG + Auto-CSV chips to the v2 Graph toolbar that setInterval the existing exportPng/exportCsv every 5 min (toggle on/off). Fixed a conflict: the auto-save chips share the #mgraph .tchip class with the time-window chips, so the window-selector handler was scoped to :not([id]) (window chips have no id) to avoid clobbering the auto-save toggles. Verified on .39: chips present, Auto-PNG toggles on, clicking a time-window chip leaves Auto-PNG untouched, 0 console errors.
<!-- SECTION:FINAL_SUMMARY:END -->
