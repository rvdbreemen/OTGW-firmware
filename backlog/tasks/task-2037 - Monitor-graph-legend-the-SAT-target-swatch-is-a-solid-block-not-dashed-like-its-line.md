---
id: TASK-2037
title: >-
  Monitor graph legend: the SAT target swatch is a solid block, not dashed like
  its line
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T22:57:00Z'
updated_date: '2026-11-19T19:04:00Z'
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

Screenshot (synthetic data, real v2.html/v2.js on fd728d6b7, headless Chrome): control setpoint, thermostat setpoint and SAT target swatches dashed, the other series solid; 0 JS exceptions. Files: C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\f1fafa03-7939-46ad-a89a-e59de20dd2bb\scratchpad\task2037-legend-dashed.png and task2037-legend-crop.png. Not yet: build and bench (coordinator: after the 08:00 beta).

Correction to the first note: the innerWidth fix landed separately as beta.395 (8cf77bb29); the TASK-2037 commit 95f898778 (beta.396) carries only the dashed swatch. Pushed 2026-11-19 (95f898778 + 7e68542ad on 22bfbb835), zip posted (Discord 1573049343798415421). Full tests/webui sweep on this v2.js: 89/89 files 0 fails (one file at a time). Bench 09 on .63 (7e68542, OTA app+fs 19:49): computed styles of the legend swatches: control setpoint, thermostat setpoint, SAT target border-top dashed, the other nine solid; the downloaded PNG legend shows the same three dashed swatches; legend toggle still works; 0 JS exceptions; 5-min smoke bootcount flash+2, reboot_log only the OTA line, MQTT up.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The legend swatch of a dashed graph series (control setpoint, thermostat setpoint, SAT target) is dashed like its line, on screen and in the PNG export (graphSeriesDashed() drives both). AC1: bench 09 on .63 beta.396 (computed border-top dashed for the three, solid for the rest; PNG legend checked) plus a synthetic screenshot. AC2: tests/webui/graph-legend-dashed.test.mjs red 2/2 on origin before the change, green 2/2 after; all 12 graph-* tests and the full tests/webui sweep (89/89) green.
<!-- SECTION:FINAL_SUMMARY:END -->
