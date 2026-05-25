---
id: TASK-707
title: 'feat-2.0.0: port TASK-703 — Statistics table draggable column widths'
status: To Do
assignee: []
created_date: '2026-05-25 23:05'
updated_date: '2026-05-25 23:08'
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
- [ ] #1 Column boundary in Statistics table is draggable in the 2.0.0 web UI
- [ ] #2 Column width preference persisted via localStorage, survives page reload
- [ ] #3 Works in Chrome, Firefox, Safari (latest-2)
- [ ] #4 python build.py (2.0.0 target) exits 0
- [ ] #5 Column boundary in Statistics table is draggable in the 2.0.0 web UI
- [ ] #6 Column width preference persisted via localStorage, survives page reload
- [ ] #7 Works in Chrome, Firefox, Safari (latest-2)
- [ ] #8 python build.py (2.0.0 target) exits 0
- [ ] #9 Column boundary in Statistics table is draggable in the 2.0.0 web UI
<!-- AC:END -->
