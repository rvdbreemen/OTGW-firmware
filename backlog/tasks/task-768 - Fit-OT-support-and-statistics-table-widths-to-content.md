---
id: TASK-768
title: Fit OT support and statistics table widths to content
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-30 13:41'
updated_date: '2026-05-30 20:53'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make the OT support and Statistics tab tables choose column widths from their widest visible text. The table width should be the sum of measured column widths, with the scroll container keeping the page within the viewport when content is wider than the available area.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each OT support and Statistics table column sizes from the widest visible header or cell text for that column.
- [x] #2 The table total width is derived from the sum of computed column widths instead of a fixed or uniform width.
- [x] #3 Existing manual column dragging remains usable after the automatic sizing changes.
- [ ] #4 When computed content-fit width is wider than the viewport/container, the OT table scroll container keeps the page contained and the cells keep their measured width instead of clipping text.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the OT support and Statistics table rendering and resize logic.\n2. Identify the current automatic column width calculation and how drag-resized widths are stored.\n3. Change automatic sizing so each column measures the widest visible header/cell text and sums those widths for the table.\n4. Keep wide tables contained with the existing horizontal scroll container instead of shrinking columns until text clips.\n5. Preserve manual drag resizing behavior and run syntax, evaluator, and rendered layout checks.\n6. Update Backlog acceptance criteria, notes, and final summary.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev (working tree already dirty before this task; unrelated existing changes were left untouched).
Agent: Codex.
Implementation: replaced the OpenTherm table min-width update with column-width application that computes each column from the widest rendered header/body text, sets the table width to the computed column sum, and proportionally constrains columns to the parent/viewport width. Preserved drag resizing and capped drag growth to the available table width. Bumped table width storage keys to avoid stale prior sizing data.
Validation: node --check src\OTGW-firmware\data\index.js; .\.venv\Scripts\python.exe evaluate.py --quick --no-color; Playwright rendered layout check at 800px viewport with normal rows, over-wide rows, and synthetic drag resize.

Follow-up 2026-05-30: user screenshot showed OT Support Boiler cells such as 'no read support...' still visually truncating, so AC #1/#3 need rework. The fix must preserve content-width columns and use horizontal scrolling when the content-fit sum is wider than the available viewport, instead of scaling columns down until text clips. Also apply the correction in the 2.0.0 worktree.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented dynamic content-fit sizing for the OT support and Statistics tables.

Changes:
- Column widths are now computed from the widest visible header/body text per column and applied through the table colgroup.
- Table width is set to the sum of computed column widths instead of filling 100% by default.
- Computed and dragged widths are capped to the available parent/viewport width so the table does not exceed the visible area.
- Manual drag resizing remains available and persists under new storage keys so stale prior sizing does not override the new fit.

Validation:
- node --check src\OTGW-firmware\data\index.js
- .\.venv\Scripts\python.exe evaluate.py --quick --no-color
- Playwright rendered layout check for normal rows, over-wide rows, and synthetic drag resize
<!-- SECTION:FINAL_SUMMARY:END -->
