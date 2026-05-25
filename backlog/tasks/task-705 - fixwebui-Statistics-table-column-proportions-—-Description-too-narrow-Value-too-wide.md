---
id: TASK-705
title: >-
  fix(webui): Statistics table column proportions — Description too narrow,
  Value too wide
status: To Do
assignee: []
created_date: '2026-05-25 20:39'
labels:
  - bug
  - webui
  - ux
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Simon Templar on #beta-testing (2026-05-25) running beta.21. The Statistics table has a small Description column and an oversized Value column, making it hard to read. Improve column proportions so Description is readable at typical browser widths.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Statistics table Description column is wide enough to show typical sensor names without truncation at 1280px browser width
- [ ] #2 Value column does not dominate the table width disproportionately
- [ ] #3 Fix uses CSS only (no JS changes); no regressions on mobile/narrow viewport
<!-- AC:END -->
