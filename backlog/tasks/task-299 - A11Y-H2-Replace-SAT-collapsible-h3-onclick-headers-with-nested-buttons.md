---
id: TASK-299
title: '[A11Y-H2] Replace SAT collapsible h3 onclick headers with nested buttons'
status: To Do
assignee: []
created_date: '2026-04-18 19:17'
labels:
  - accessibility
  - review-2026-04-18
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
data/index.html:456 and :566 use <h3 onclick=SAT.toggleCurve/toggleRawData>. Not focusable, Tab skips them, WCAG 2.1.1 fail. No aria-expanded.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Both headings contain a <button type=button aria-expanded aria-controls=...>
- [ ] #2 toggleCurve and toggleRawData sync aria-expanded state
- [ ] #3 Keyboard Tab reaches both controls and Enter/Space activates them
<!-- AC:END -->
