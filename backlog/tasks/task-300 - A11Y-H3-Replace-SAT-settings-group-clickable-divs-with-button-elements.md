---
id: TASK-300
title: '[A11Y-H3] Replace SAT settings group clickable divs with button elements'
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
index.js:3530 buildSATSettingsGroups uses clickable <div> with nested Save <button>. Not keyboard reachable, nested interactive elements.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Collapse control becomes <button type=button aria-expanded aria-controls>
- [ ] #2 Save button is a sibling of the collapse button, not nested
- [ ] #3 toggleSATSettingsGroup updates aria-expanded
<!-- AC:END -->
