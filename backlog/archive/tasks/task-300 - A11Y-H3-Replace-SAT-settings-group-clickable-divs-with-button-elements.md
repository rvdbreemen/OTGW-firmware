---
id: TASK-300
title: '[A11Y-H3] Replace SAT settings group clickable divs with button elements'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:17'
updated_date: '2026-04-19 06:33'
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
- [x] #1 Collapse control becomes <button type=button aria-expanded aria-controls>
- [x] #2 Save button is a sibling of the collapse button, not nested
- [x] #3 toggleSATSettingsGroup updates aria-expanded
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index.js buildSATSettingsGroups now emits a <button class=sat-settings-group-toggle aria-expanded aria-controls> inside the header div, with the Save button as a sibling (no longer nested in a clickable container). toggleSATSettingsGroup syncs aria-expanded on the toggle button. CSS cursor:pointer moved from the outer header div to the toggle button so only the button acts as a click target; focus ring via focus-visible. The click target for Save no longer accidentally triggers the collapse.
<!-- SECTION:FINAL_SUMMARY:END -->
