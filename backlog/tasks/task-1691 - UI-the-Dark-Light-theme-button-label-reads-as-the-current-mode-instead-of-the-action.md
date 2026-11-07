---
id: TASK-1691
title: >-
  UI: the Dark/Light theme button label reads as the current mode instead of the
  action
status: Done
assignee: []
created_date: '2026-11-07T11:37:00Z'
updated_date: '2026-11-07T16:55:00Z'
labels:
  - needs-maintainer-ok
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 807000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs in #alpha-testing 2026-11-07 10:06-10:07 UTC (msgs 1568567278998388757, 1568567443079561289): the button labelled 'Dark' means 'switch to dark' while the UI is light, which reads as 'this is dark mode'; suggestion: label 'To dark' or show the current mode. Tester input; label choice is Robert's call.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision by Robert on the label wording (action vs current state)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-07: closed as covered by TASK-1692 (action label kept, aria-pressed dropped, aria-label/title 'Switch to light/dark theme'; pushed as beta.124). A different visible label would be a new small task.
<!-- SECTION:NOTES:END -->
