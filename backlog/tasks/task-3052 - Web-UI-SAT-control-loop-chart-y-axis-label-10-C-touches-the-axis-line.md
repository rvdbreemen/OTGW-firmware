---
id: TASK-3052
title: 'Web UI SAT control-loop chart: y-axis label -10 C touches the axis line'
status: In Progress
assignee:
  - '@otgw-ux-designer'
created_date: '2026-11-25T10:06:00Z'
updated_date: '2026-11-27T05:17:00Z'
labels:
  - webui
  - ux
milestone: 2.1.0
dependencies: []
ordinal: 1155000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen in the bench screenshot S15 (OTGW-field-data/bench-63/2026-11-25_beta-candidate/screenshots/S15-light-1400.png): in the SAT control loop chart the first panel y label -10 C sits against the left axis line. Cosmetic, not a beta blocker.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No y-axis label touches or crosses the axis line in the SAT control-loop chart, light and dark, at 390 and 1400 px
- [ ] #2 Browser test or screenshot as evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 otgw-ux-designer, diagnosis from bench screenshot S15-light-1400.png and v2.js drawPanelChart (~3659): the y value labels are left-aligned at x=2 and the axis line is at C_X0=34 (v2.js:3283). In a 10 px mono font a 5-character label such as '-10°C' is about 30 px wide, so it ends at about x=32 and leaves 2 px to the axis: it touches the line. Shorter labels ('30°C' 4 chars) leave 8 px, which is why only the -10 label looks wrong. The chart is shared with Monitor > Graph, so the fix applies to both. Fix in two parts. CSS (mine, in wt-ux, uncommitted until the bump commit): .axis-txt.yl{text-anchor:end} after .axis-txt.gtitle in v2.css. JS (otgw-webui): in drawPanelChart replace the value-label calls txtEl(2, ..., label) by txtEl(C_X0 - 4, ..., label, 'yl') for the three temperature labels, the '100%' and '0%' labels and the lane labels (graph_lane_on / graph_lane_off); the gtitle at x=38 stays. Result: every y label ends 4 px left of the axis whatever its length; a 5-character label starts at x=0 and is not clipped. Test: tests/webui, render the SAT page with satHist values spanning -10 to 30 and assert for every .axis-txt.yl that getBBox().x + width <= C_X0 - 3 and x >= 0; red on the current v2.js. Not verified: not run, v2.js is otgw-webui's file; the CSS alone has no effect until the JS passes the class.
<!-- SECTION:NOTES:END -->
