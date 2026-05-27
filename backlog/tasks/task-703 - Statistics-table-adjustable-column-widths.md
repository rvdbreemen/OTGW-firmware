---
id: TASK-703
title: 'Statistics table: adjustable column widths'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-25 17:16'
updated_date: '2026-05-26 09:18'
labels:
  - ui
  - enhancement
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Simon Templar (beta.21 feedback): Description column is too small, Value column too wide in Statistics tab. Add resizable columns so users can drag the column boundary to their preference.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Column boundary in Statistics table is draggable (mouse drag on column header edge)
- [x] #2 Column width preference is persisted via localStorage so it survives page reload
- [x] #3 Works in Chrome, Firefox, Safari (latest-2)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Add drag-to-resize column widths to Statistics table. Implemented via colgroup/col elements and mousedown/mousemove/mouseup drag handles on th right edges. Widths persisted to localStorage key otStatsColWidths. Restored on page load and tab switch. Global col-resizing cursor class prevents text selection mid-drag. Minimum column width 30px. Works cross-browser. No external dependencies. Build green, evaluator 100%.
<!-- SECTION:FINAL_SUMMARY:END -->
