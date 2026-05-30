---
id: TASK-767
title: Normalize resizable OT Support and Statistics columns
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-30 11:37'
updated_date: '2026-05-30 11:39'
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
- [ ] #1 OT Support table columns can be resized by dragging column boundaries using the same interaction model as the Statistics table.
- [ ] #2 OT Support default column widths are fit from the widest header/cell content so visible content fits cleanly without avoidable clipping.
- [ ] #3 Statistics default column widths are fit from the widest header/cell content so visible content fits cleanly without avoidable clipping.
- [ ] #4 Both tables remain usable on narrower viewports with horizontal scrolling rather than layout breakage.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the OT Support and Statistics table code and identify the existing Statistics resize behavior.
2. Reuse or generalize that resize behavior for OT Support rather than adding a separate interaction.
3. Add content-fit default width calculation for both tables, preserving user drag overrides and horizontal scrolling.
4. Validate with focused static/build checks and, if practical, inspect the UI behavior in a browser.
<!-- SECTION:PLAN:END -->
