---
id: TASK-1863
title: Home Mission control keeps a fixed dark palette in the light theme
status: To Do
assignee: []
created_date: '2026-11-12T08:06:00Z'
labels:
  - webui
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 939000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench finding by otgw-bench-tester (09) on 2.0.0-beta.208+e449692 (TASK-1841 bench, 2026-11-12): Home > Mission control (.mc block, v2.css:268, already on origin/dev before TASK-1841) keeps its own fixed dark palette, so it stays dark in the light theme while the SAT control loop chart and the rest of the page follow the theme. Unknown whether that is a deliberate cockpit look or an oversight; needs Robert's choice: (A) Mission control follows the theme like the other panels, (B) keep it dark on purpose and say so. Screenshots: 09 scratchpad b208/ui61-mission-light.png vs ui61-mission-dark.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's choice (A follow the theme / B stay dark on purpose) recorded in the notes
- [ ] #2 If A: Mission control uses the theme tokens in light and dark; test red on the old code; screenshots light and dark
- [ ] #3 If B: a comment at the .mc palette and a line in the UI docs say it is dark on purpose
<!-- AC:END -->
