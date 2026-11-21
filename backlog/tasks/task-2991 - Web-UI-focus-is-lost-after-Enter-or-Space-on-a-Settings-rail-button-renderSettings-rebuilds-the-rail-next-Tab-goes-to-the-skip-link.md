---
id: TASK-2991
title: >-
  Web UI: focus is lost after Enter or Space on a Settings rail button
  (renderSettings rebuilds the rail; next Tab goes to the skip link)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T02:07:00Z'
updated_date: '2026-11-21T02:12:00Z'
labels:
  - webui
  - a11y
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1116000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench finding by otgw-bench-tester on .63 (TASK-2982, beta.405+d2a9aee, evidence OTGW-field-data/bench-63/2026-11-21_task2982-ariacurrent/ui/t2982-ui-63/facts.json). Pre-existing since TASK-2958 (beta.404) made the rail items <button>: a headless probe on beta.404 and beta.405 both leave document.activeElement on <body> after Enter on a focused rail button. Cause: the click handler sets setActiveCat and calls renderSettings(), which empties #setRail (v2.js rail.textContent = '') and builds new buttons, so the focused element no longer exists. A keyboard user then starts over at the skip link (WCAG 2.4.3 focus order). Fix idea: before the rebuild remember the focused rail category, after the rebuild focus the new button with that data-cat.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After Enter or Space on a focused rail button, focus is on the rail button of the opened category (browser test: red on the current code, green on the fix)
- [ ] #2 Search typing in the settings search field keeps its focus (no regression); full tests/webui sweep green
- [ ] #3 Short bench on a unit (otgw-bench-tester, NAS Chromium)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-webui: renderSettings() remembers the focused rail category (document.activeElement inside #setRail) before it empties the rail and focuses the rebuilt button of that category afterwards; focus elsewhere (the settings search) is untouched. Test a11y-structure.test.mjs: after Enter and after Space on a rail button, document.activeElement is the rail button of the opened category: red on the current code (actual BODY, expected mqtt), green on the fix; typing 'mq' in #setSearch keeps focus and value in the search field (green). a11y-structure 12/12, settings-help 29/29. Full sweep, build and bench pending (heavy slot after the 08:00 beta).
<!-- SECTION:NOTES:END -->
