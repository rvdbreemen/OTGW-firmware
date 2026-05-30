---
id: TASK-768
title: Fit OT support and statistics table widths to content
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-30 13:41'
updated_date: '2026-05-30 13:43'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make the OT support and Statistics tab tables choose column widths from their widest visible text while keeping the total table width within the available viewport. Manual drag resizing can remain available but should not be required for a good initial fit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each OT support and Statistics table column sizes from the widest visible header or cell text for that column.
- [ ] #2 The table total width is derived from the sum of computed column widths instead of a fixed or uniform width.
- [ ] #3 Computed table width is capped to the available viewport/container width so the table never exceeds the visible area.
- [ ] #4 Existing manual column dragging remains usable after the automatic sizing changes.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the OT support and Statistics table rendering and resize logic.
2. Identify the current automatic column width calculation and how drag-resized widths are stored.
3. Change automatic sizing so each column measures the widest visible header/cell text, sums column widths for the table, and caps the result to the available viewport/container width.
4. Preserve manual drag resizing behavior and run the relevant UI validation/build checks.
5. Update task notes, acceptance criteria, Definition of Done, and final summary.
<!-- SECTION:PLAN:END -->
