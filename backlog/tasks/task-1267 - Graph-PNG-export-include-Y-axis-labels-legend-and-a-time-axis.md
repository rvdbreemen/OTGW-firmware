---
id: TASK-1267
title: 'Graph PNG export: include Y-axis labels, legend and a time axis'
status: To Do
assignee: []
created_date: '2026-10-20 18:02'
labels:
  - feature
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 397000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Request from gruijter (#alpha-testing 2026-10-20) with screenshot. exportPng() (data/v2.js ~2975-2985) serializes only the #gstrip SVG into an <img> and draws it on a dark canvas. The Y-axis labels are <text class=axis-txt> inside the SVG but take fill/font from v2-bundle.css (.axis-txt, ~564), which an SVG-as-image cannot load, so they render black on #0d1117 (invisible). The legend (#gLegend) is an HTML div outside the SVG, so it is never exported. No time (X) axis exists, on screen or in the PNG.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Exported PNG shows readable Y-axis labels, the legend of the visible series and a time axis (screenshot of an exported PNG, old vs fix)
- [ ] #2 On-screen graph unchanged apart from the added time axis (screenshot)
<!-- AC:END -->
