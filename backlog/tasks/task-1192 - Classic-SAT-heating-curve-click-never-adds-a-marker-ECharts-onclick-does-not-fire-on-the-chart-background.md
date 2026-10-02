---
id: TASK-1192
title: >-
  Classic SAT heating-curve click never adds a marker: ECharts on('click') does
  not fire on the chart background
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-02 05:51'
updated_date: '2026-10-02 08:55'
labels:
  - bug
  - webui
  - sat
dependencies: []
priority: medium
ordinal: 314000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1172 AC#6 bench run (2026-10-02, OTGW32, alpha.397 app + LittleFS). In the classic UI, SAT page in Expert view, the heating curve renders (#sat-curve-chart 1368x320, ECharts instance present) and grid point (outside 5 C, flow 50 C) maps to pixel (688, 128). A real mouse click there sends no POST /api/v2/sat/markers, and a listener added with inst.on('click') receives nothing when the zrender layer gets a background click. sat.js _initCurveClickHandler() registers _curveChartInstance.on('click', ...) and filters componentType !== 'series', but ECharts only emits instance-level click events for graphic elements, never for empty grid space; background clicks reach only inst.getZr().on('click'). So addMarkerAtClick() is unreachable and the 'add a calibration marker by clicking the heating curve' feature from TASK-586 (AC#1 checked 2026-05-08) does not work. The v2 UI only reads markers (v2.js:3108), so this is classic-only. Fix plan: register the handler on getZr(), skip clicks outside the grid (containPixel('grid', [offsetX, offsetY])) and clicks on a series element (event target present), then convertFromPixel and the existing clamp + addMarkerAtClick(). Evidence: %LOCALAPPDATA%/OTGW-capture/task1172-bench-20261002/clickprobe.py and t1172-FIX-ui-p2-*.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OLD-vs-FIX in a real browser on the bench (Playwright/CDP): with the OLD sat.js a click on grid point (5, 50) of the heating curve sends no POST; with the FIX it sends POST /api/v2/sat/markers with outside_temp about 5 and flow_temp about 50, the response is 201 and the marker appears in #sat-marker-list
- [ ] #2 With the FIX a click on an existing marker's diamond (a non-silent scatter element, so the zrender event carries a target) and a click outside the grid area add no marker. The curve lines are silent: a click on a curve adds a marker at that point, as TASK-586 intends
- [ ] #3 The added marker is removed with its x button: DELETE /api/v2/sat/markers/<id> returns 200 and the list shows the previous count
- [ ] #4 build.bat for esp32-combo prints its SUCCESS line for firmware and filesystem; python evaluate.py --quick shows no new failures; the change lands in one commit with its own prerelease bump
- [ ] #5 Drawn markers stay on the heating curve across SAT status polls: on the bench the Markers series keeps every stored marker for at least 3 polls after an add
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-02: AC#2 rewritten before implementation. The first version said a click on a curve point adds no marker, but every curve series is silent: true (buildCurveOption), so it takes no hits and a click on a curve is a background click. The non-silent elements are the Markers scatter and the Current point. Fix: one named handler on getZr(), registered off-then-on per chart instance (echarts.init() returns the existing instance on each SAT open, so on() handlers piled up), and registered again after setTheme() rebuilds the curve chart; before this, a theme switch left no click handler at all.

2026-10-02 bench, alpha.398 (the zrender click fix):
- The click now adds a marker. On a fresh SAT open in Expert view, a click aimed at (5, 50) posted {outside_temp 5, flow_temp 49.9}, answered 201.
- Earlier tries posted (4.7, 53.3). That was a test error: #sat-curve-chart has 10 px padding plus a 1 px border, and the test added canvas pixels to the container origin. Measured: viewport click (327, 878) gives native offset (300, 150), and zrender offset (300, 150) matches exactly.

Second defect in the same feature, found while checking AC#2: drawn markers vanish from the curve on the next status poll.
- updateCurveChart()'s partial path updated the last two series by position, assuming they are Curve Pos and Current.
- _renderMarkersOnChart() appends Markers after them, so the Current point data (empty on the bench) overwrote the markers.
- Measured on alpha.398 (vanish.py): series order [..., Curve Pos, Current, Markers]. The Markers series had 4 points right after an add and 0 after the next sat/status poll, while the list kept showing 4.
- Fix: update Curve Pos and Current by series name, guarded on both existing; added as AC#5.

Separate defect, not in this task: re-opening the SAT page through Home -> SAT leaves both dashboard charts blank. elementFromPoint at the curve hits the container div, not the canvas, so clicks never reach zrender. Filed as its own task.
<!-- SECTION:NOTES:END -->
