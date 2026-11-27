---
id: TASK-3125
title: 'Web UI: phone chart text too small (SVG viewBox scaling)'
status: To Do
assignee: []
created_date: '2026-11-27T13:37:00Z'
labels:
  - webui
milestone: 2.1.0
dependencies: []
ordinal: 1034000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On a 375 px phone the Monitor graph SVG (viewBox about 780 units wide) scales to about 0.48, so all chart text (y-axis labels, panel names) renders near 5 px and is unreadable. Seen in the TASK-3052 evidence screenshots (ux review 2026-11-27). Design item: a phone breakpoint with larger text in viewBox units or a narrower viewBox; not a TASK-3052 regression.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 At 375 px chart labels render at 10 px or more on screen
- [ ] #2 No overlap of labels with plotted data at 375 px and 1400 px; screenshots before and after
- [ ] #3 One-file web UI test for the chosen rule
<!-- AC:END -->
