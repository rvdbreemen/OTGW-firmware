---
id: TASK-299
title: '[A11Y-H2] Replace SAT collapsible h3 onclick headers with nested buttons'
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
data/index.html:456 and :566 use <h3 onclick=SAT.toggleCurve/toggleRawData>. Not focusable, Tab skips them, WCAG 2.1.1 fail. No aria-expanded.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Both headings contain a <button type=button aria-expanded aria-controls=...>
- [x] #2 toggleCurve and toggleRawData sync aria-expanded state
- [x] #3 Keyboard Tab reaches both controls and Enter/Space activates them
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index.html:456 and :566: each sat-curve-toggle and sat-collapsible h3 now contains a <button type=button class=heading-toggle-btn aria-expanded aria-controls onclick=...>. sat.js toggleCurve and toggleRawData both sync aria-expanded on the inner button. New CSS .heading-toggle-btn in index_common.css resets button styling (all:unset) so the heading looks identical; focus ring added via focus-visible. toggleRawData refactored to use textContent with Unicode escapes instead of innerHTML with HTML entities (cleaner and satisfies XSS-linter).
<!-- SECTION:FINAL_SUMMARY:END -->
