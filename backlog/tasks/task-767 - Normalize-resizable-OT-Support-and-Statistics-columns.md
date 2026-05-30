---
id: TASK-767
title: Normalize resizable OT Support and Statistics columns
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-30 11:37'
updated_date: '2026-05-30 11:38'
labels:
  - ui ot-support
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fix the OT Support tab table so columns can be resized by dragging like the Statistics tab, and make both OT Support and Statistics tables size their default columns from the widest content so values fit cleanly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OT Support table columns can be resized by dragging column boundaries.
- [ ] #2 Default OT Support column widths are normalized so visible content fits cleanly without avoidable clipping.
- [ ] #3 The table remains usable on narrower viewports with horizontal scrolling rather than layout breakage.
- [ ] #4 OT Support table columns can be resized by dragging column boundaries using the same interaction model as the Statistics table.
- [ ] #5 OT Support default column widths are fit from the widest header/cell content so visible content fits cleanly without avoidable clipping.
- [ ] #6 Statistics default column widths are fit from the widest header/cell content so visible content fits cleanly without avoidable clipping.
- [ ] #7 Both tables remain usable on narrower viewports with horizontal scrolling rather than layout breakage.
<!-- AC:END -->
