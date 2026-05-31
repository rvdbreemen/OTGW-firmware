---
id: TASK-771
title: Stop table auto-resize on every tick
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 07:44'
updated_date: '2026-05-31 07:45'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the Web UI table sizing code and the tick/update path that calls it.
2. Change recalculation so it runs only after relevant table row/content changes.
3. Validate with the smallest available UI/static test or targeted code inspection, and document any non-automated verification gap.
<!-- SECTION:PLAN:END -->
