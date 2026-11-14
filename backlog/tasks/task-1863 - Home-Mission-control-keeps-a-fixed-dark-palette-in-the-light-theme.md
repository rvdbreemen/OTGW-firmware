---
id: TASK-1863
title: Home Mission control keeps a fixed dark palette in the light theme
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T08:06:00Z'
updated_date: '2026-11-14T21:39:00Z'
labels:
  - webui
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
- [x] #1 Robert's choice (A follow the theme / B stay dark on purpose) recorded in the notes
- [x] #2 If A: Mission control uses the theme tokens in light and dark; test red on the old code; screenshots light and dark
- [ ] #3 If B: a comment at the .mc palette and a line in the UI docs say it is dark on purpose
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 Robert (DM 1571274595758248078, relayed by the coordinator, answer 'A'): A, follow the theme. Label needs-maintainer-ok removed.

2026-11-14 (92, lane wt-92-webui), Robert's A applied. v2.css .mc no longer redefines --bg/--panel/--panel-2/--border/--text/--muted/--gauge-track; it gets a theme border; the .mc-only hard-coded colours move to tokens: .mc h2 b and .mc-cell .val.ok #79f2c0 -> var(--zone-ok); .ticker background #0a0e14 -> var(--panel), .ticker .t #00bffe -> var(--accent), .b #79f2c0 -> var(--zone-ok), .ts #566069 -> var(--muted). Out of scope (outside Mission control, unchanged): .console/.cmdbar-prompt/.lcd-txt/.suptip. New test tests/webui/mission-control-follows-theme.test.mjs (real v2.html/v2.css, light and dark): OLD 0 pass 2 fail ('.mc background rgb(13, 17, 23), theme --bg rgb(241, 247, 248)' in light); FIX 2 pass 0 fail. graph-time-axis-and-png-export.test.mjs 11/11. evaluate.py --quick 0 FAIL. Screenshots (Home > View: Mission control, 1280 px, harness): comms scratchpad t1863/old-mission-light.png (dark box in light theme), fix-mission-light.png, fix-mission-dark.png.
<!-- SECTION:NOTES:END -->
