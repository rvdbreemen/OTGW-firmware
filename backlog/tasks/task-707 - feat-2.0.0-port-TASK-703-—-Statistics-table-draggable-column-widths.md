---
id: TASK-707
title: 'feat-2.0.0: port TASK-703 — Statistics table draggable column widths'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-25 23:05'
updated_date: '2026-05-26 09:47'
labels:
  - ui
  - feat-2.0.0
  - port
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the draggable column-width feature from dev TASK-703 to the 2.0.0 feature line. Identical change: colgroup/col elements in index.html, drag-handle JS in index.js, resize-cursor CSS in index.css. localStorage key otStatsColWidths persists widths across reloads. Pure frontend — no firmware .ino changes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Column boundary in Statistics table is draggable in the 2.0.0 web UI
- [x] #2 Column width preference persisted via localStorage, survives page reload
- [x] #3 Works in Chrome, Firefox, Safari (latest-2)
- [x] #4 python build.py (2.0.0 target) exits 0
- [x] #5 Column boundary in Statistics table is draggable in the 2.0.0 web UI
- [ ] #6 Column width preference persisted via localStorage, survives page reload
- [ ] #7 Works in Chrome, Firefox, Safari (latest-2)
- [ ] #8 python build.py (2.0.0 target) exits 0
- [ ] #9 Column boundary in Statistics table is draggable in the 2.0.0 web UI
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of TASK-703 Statistics table draggable columns to 2.0.0 feature branch.\n\nChanges:\n- index.html: added <colgroup> with 6 <col> elements inside #otStatsTable\n- components.css: added .col-resizer styles using CSS design tokens (var(--border-1)), body.col-resizing cursor override\n- index.js: all 8 resize functions inserted before statsBuffer; initStatsColResizers() called from initMainPage() and Statistics branch in openLogTab()\n\nBuild: python build.py exit 0. Evaluator: 98.6%, 0 failures.\nBranch: feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:FINAL_SUMMARY:END -->
