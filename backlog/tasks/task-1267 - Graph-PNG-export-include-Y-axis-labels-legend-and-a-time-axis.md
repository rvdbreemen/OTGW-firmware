---
id: TASK-1267
title: 'Graph PNG export: include Y-axis labels, legend and a time axis'
status: Done
assignee:
  - '@claude'
created_date: 2026-10-20 18:02
updated_date: '2026-10-22T03:03:00Z'
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
- [x] #1 Exported PNG shows readable Y-axis labels, the legend of the visible series and a time axis (screenshot of an exported PNG, old vs fix)
- [x] #2 On-screen graph unchanged apart from the added time axis (screenshot)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) Time axis (on screen and in PNG): v2.html #gstrip viewBox 0 0 780 250 -> 780 262, add <g id=gTime>. renderGraph() fills it each pass: tick step chosen from 1,2,5,10,15,30 min,1,2,3,6,12,24 h so the window (graphWindowMs, t0 = now - win) gets 4..8 ticks, aligned to local wall-clock multiples; per tick a faint vertical gridline (y 10..244) and an HH:MM label (axis-txt) at y 257, x = C_X0 + (tick - t0)/win*span; labels near the edges clamped/skipped so they do not clip. Day-wide windows show the date (dd-mm) on ticks at midnight. 2) exportPng(): clone #gstrip, copy computed fill/font-size/font-family of every <text> onto the clone as attributes (CSS does not load inside an SVG image), set width/height = viewBox; canvas 2x scale, height = legend band 26 + 262; draw the legend band from the visible #gLegend entries (swatch colour + label, hidden series opacity .4 skipped) with canvas 2D text, then the SVG below it. Background stays #0d1117, text colour from the computed --muted / --text. 3) Test: tests/webui node test calling the real tick helper (extracted as a pure function inside v2.js, exported via the existing test hook pattern) for 10 min / 1 h / 6 h / 24 h windows; source-level check that exportPng inlines text styles and draws the legend. 4) Evidence: Playwright on the bench (.88.61): export PNG with the old v2.js (red: labels invisible, no legend, no time axis) vs fix uploaded to LittleFS (green), plus on-screen screenshot old vs new. No firmware change, no API call added (ADR-165 untouched). Cross-line: v2 UI only exists on dev.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 RED (bench .88.61, alpha.450 filesystem, /simulate replay, 10 min window, headless Chromium): exported PNG shows axis labels in black default serif on #0d1117 (barely visible), no legend, no time axis; on screen (light theme) legend present, no time axis. Also found: export background is hard-coded dark regardless of theme. Evidence %TEMP%\bench-1267\red-export.png, red-screen.png.

2026-10-22 review (11 agents: correctness opus, rules, tests; 2 skeptics per finding): correctness and rules found nothing. Tests lens: (1) graphTimeTicks repeated/reordered ticks over spring-forward (2/2 confirmed; reproduced: 10 min at 2026-03-29 01:01Z gave 01:52..01:58 twice), (2) lost the repeated hour over fall-back (2/2), (3) DST test too weak (2/2), (4) layout assertions weak (1/2). All fixed: ticks are now real instants on a UTC grid (step, or 30 min for hour steps) filtered on the local hour, so they can only increase; new DST sweep test over both 2026 changes for the 10 min/1 h/4 h/24 h windows (1388 windows; strictly increasing, unique, round clock times, no gap > 2 steps, 4..9 ticks: 9 is correct on the spring-forward day, 25 wall-clock hours in 24 h); 4 h chip added to the window list; label/gridline layout assertions added. Evidence: fix 11/11 pass; first version red ('repeated tick' at 2026-03-29T01:01:07Z); HEAD 0/11; tests/webui 114/114; node --check OK.

2026-10-22 GREEN (bench .88.61, filesystem of this diff via /update?cmd=100, settings diff after the update empty; /simulate replay, 10 min window, headless Chromium): exported PNG (2x, 1560x608) shows the legend of the visible series (2 rows), readable axis labels, gridlines and HH:MM time labels 04:52..05:00 on 2-minute steps; background and text follow the theme (light: white card; dark: dark card, light text). On-screen graph identical to the red screenshot apart from the added gridlines and time labels. Axis labels in the PNG use the browser's monospace fallback (the web font cannot load inside an SVG image); readable. Gates: evaluate --quick 72/0, tests/webui 114/114, node --check OK, build.bat esp32-combo SUCCESS. Docs: ch03/h03 graph line. Evidence %TEMP%\bench-1267\green-light-export.png, green-dark-export.png, green-light-screen.png vs red-*.png.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Monitor > Graph has a time axis (gridlines and HH:MM labels on round clock times; the step follows the window, dd-mm on midnight for 24 h), and the PNG export now matches what you see: readable axis labels (computed styles copied into the exported SVG), the legend of the visible series, the time axis, rendered at 2x in the current theme. AC1: bench PNG red (black serif labels, no legend, no time axis) vs green in light and dark theme. AC2: on-screen screenshot old vs new differs only by the time axis. Review (11 agents) found DST bugs in the first tick algorithm (repeated ticks at spring-forward, lost hour at fall-back); fixed by walking real UTC instants and filtering on the local hour, with a 1388-window DST sweep test that is red on the first version. Not verified: Firefox and Safari rendering of the export, the Nepal +5:45 zone (30-min ticks would sit on :15/:45).
<!-- SECTION:FINAL_SUMMARY:END -->
