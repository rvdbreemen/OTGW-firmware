---
id: TASK-771
title: Stop table auto-resize on every tick
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 07:44'
updated_date: '2026-05-31 07:44'
labels:
  - bug web-ui
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fix the table layout regression where the table width grows on every UI tick even when rows or content have not changed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Table width recalculation runs only when relevant table structure/content changes, not on every tick.
- [ ] #2 Existing table updates still render correctly when rows are added or changed.
- [ ] #3 Validation covers the resize regression or documents why it cannot be directly automated.
<!-- AC:END -->
