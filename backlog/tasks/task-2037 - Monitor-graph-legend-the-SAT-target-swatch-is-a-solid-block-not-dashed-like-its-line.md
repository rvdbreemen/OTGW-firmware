---
id: TASK-2037
title: >-
  Monitor graph legend: the SAT target swatch is a solid block, not dashed like
  its line
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T22:57:00Z'
updated_date: '2026-11-19T17:10:00Z'
labels:
  - webui
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1045000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw-bench-tester observation on the TASK-2018 bench (.63, beta.378+1fcc894, 2026-11-18): the Monitor graph draws the SAT target series dashed (5 4, #e0c341), but its legend entry shows a solid colour block, so the legend does not say which line is dashed. Not a finding against TASK-2018 (its ACs hold); a small polish item.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The legend swatch of a dashed series (SAT target) shows the dash pattern of its line
- [x] #2 Web UI test on the real v2.js: red on the code before, green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 (ba). Fix in v2.js: graphSeriesDashed(k) (chSet, roomSet, satTarget) drives both the 5 4 line dash and the legend swatch; a dashed series gets swatch background none + border-top 2px dashed in its colour, the same form the mark legend (TASK-1914) already used. PNG export: legendEntries() reads the colour from the top border when the background is none and reports dash; layoutLegend() carries it; exportPng() paints a dashed swatch as two 5 px segments with a 4 px gap. That also fixes the PNG legend for the dashed mark entries, whose 'none' background was passed to fillStyle. New tests/webui/graph-legend-dashed.test.mjs: (1) real page in headless Chrome, SAT enabled with satTarget history: SAT target swatch computed border-top-style dashed and transparent background, line stroke-dasharray 5 4, boiler swatch solid; (2) legendEntries + layoutLegend cut out of v2.js. Red on origin/dev data (2/2 fail: swatch bg rgb(224,195,65), border none; legendEntries without dash), green on the fix (2/2). All 12 graph-* web UI tests green. Same commit: v2-undefined-names failed on origin since TASK-2918 (ffe5dffeb) on 'innerWidth (line 8639)', a bare global in the popover clamp; now window.innerWidth; v2-undefined-names 2/2, update-check-github 14/14, update-ask-chip-phone 1/1.
<!-- SECTION:NOTES:END -->
