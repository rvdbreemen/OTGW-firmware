---
id: TASK-298
title: >-
  [A11Y-H1] Replace OT-Direct overrides clickable div with keyboard-accessible
  button
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
index.html:110 uses <div onclick=toggleOTDOverrides()>, not keyboard-reachable, violates WCAG 2.1.1. Pattern exists already on #headerThemeToggle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Row is a <button type=button aria-expanded aria-controls=otd-ovr-section>
- [ ] #2 toggleOTDOverrides updates aria-expanded on show/hide
- [ ] #3 Tab reaches the control, Enter and Space toggle it
<!-- AC:END -->
