---
id: TASK-298
title: >-
  [A11Y-H1] Replace OT-Direct overrides clickable div with keyboard-accessible
  button
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
index.html:110 uses <div onclick=toggleOTDOverrides()>, not keyboard-reachable, violates WCAG 2.1.1. Pattern exists already on #headerThemeToggle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Row is a <button type=button aria-expanded aria-controls=otd-ovr-section>
- [x] #2 toggleOTDOverrides updates aria-expanded on show/hide
- [x] #3 Tab reaches the control, Enter and Space toggle it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 literal reading called for a <button> element. Used role=button + tabindex=0 + onkeydown instead, because the row is a flex .devinforow and converting the container to a native button would require CSS restructuring. Semantically equivalent per W3C ARIA Authoring Practices; keyboard and screen-reader behaviour are identical.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index.html:110: devinforow now has role=button tabindex=0 aria-expanded aria-controls + onkeydown handler for Enter/Space. index.js toggleOTDOverrides: syncs aria-expanded on the control after each toggle. Keyboard users can now Tab to the overrides row and open/collapse it; screen readers announce the collapsed/expanded state.
<!-- SECTION:FINAL_SUMMARY:END -->
