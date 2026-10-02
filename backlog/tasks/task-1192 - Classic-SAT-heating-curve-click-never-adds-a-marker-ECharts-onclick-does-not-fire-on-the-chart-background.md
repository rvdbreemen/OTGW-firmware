---
id: TASK-1192
title: >-
  Classic SAT heating-curve click never adds a marker: ECharts on('click') does
  not fire on the chart background
status: Done
assignee:
  - '@claude'
created_date: '2026-10-02 05:51'
updated_date: '2026-10-02 09:03'
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
- [x] #1 OLD-vs-FIX in a real browser on the bench (Playwright/CDP): with the OLD sat.js a click on grid point (5, 50) of the heating curve sends no POST; with the FIX it sends POST /api/v2/sat/markers with outside_temp about 5 and flow_temp about 50, the response is 201 and the marker appears in #sat-marker-list
- [x] #2 With the FIX a click on an existing marker's diamond (a non-silent scatter element, so the zrender event carries a target) and a click outside the grid area add no marker. The curve lines are silent: a click on a curve adds a marker at that point, as TASK-586 intends
- [x] #3 The added marker is removed with its x button: DELETE /api/v2/sat/markers/<id> returns 200 and the list shows the previous count
- [x] #4 build.bat for esp32-combo prints its SUCCESS line for firmware and filesystem; python evaluate.py --quick shows no new failures; the change lands in one commit with its own prerelease bump
- [x] #5 Drawn markers stay on the heating curve across SAT status polls: on the bench the Markers series keeps every stored marker for at least 3 polls after an add
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Clicking the empty heating-curve grid on the classic SAT page never added a calibration marker. Markers drawn on the curve also vanished at the next status poll. Both defects are fixed.

Click (a6303b65e, alpha.398):
- Cause: _initCurveClickHandler() used chart.on('click'). ECharts emits instance-level click events only for graphic elements, never for empty grid space.
- Fix: one named handler on getZr(). It skips clicks that hit an element (e.target) and clicks outside the grid (containPixel), then converts, clamps and posts as before.
- The handler is registered off-then-on, once per chart instance, because echarts.init() returns the existing instance on every SAT open. It is registered again after setTheme(), which used to drop it.

Vanishing markers (6d0d20d19, alpha.399):
- Cause: updateCurveChart()'s partial path updated the last two series by position. The Markers series sits after Curve Pos and Current, so each poll wrote the current point into the markers.
- Fix: Curve Pos and Current are now updated by series name, only when both exist.

Evidence. Bench OTGW32, headless Edge through Playwright (CDP). Fresh SAT open, Expert view. %LOCALAPPDATA%/OTGW-capture/task1192-bench-20261002/.

AC#1 (old vs fix):
- OLD alpha.397 (t1192-OLD-result.json, clickprobe.py): a real click on grid point (5, 50) sent no POST, and an inst.on('click') listener received nothing.
- FIX alpha.399 (t1192c-FIX399-result.json): POST {outside_temp 5, flow_temp 49.9} answered 201 id 2, and the label showed in #sat-marker-list.
- Earlier tries posted (4.7, 53.3). That was a test error: the canvas sits 11 px inside the padded container. offsetprobe.py shows the viewport click mapping exactly to zrender's offset (300, 150).

AC#2:
- A click outside the grid sent no POST.
- Clicks on both drawn diamonds, (15, 35) and (5, 49.9), sent no POST.
- The curve lines are silent, so a click on a curve adds a marker there, as TASK-586 intends. The AC was reworded to this before implementation.

AC#3: the x button sent DELETE /api/v2/sat/markers/2 (the id from the POST answer), answered 200, and the label left the list.

AC#4:
- build.bat --target esp32-combo, firmware SUCCESS and filesystem SUCCESS, 'Build completed successfully!':
  - alpha.398+a6303b6: 198.5 s + 23.7 s;
  - alpha.399+6d0d20d: 194.3 s + 24.0 s.
- The images were flashed with --update --app --fs, and the fs version.hash matched each build.
- evaluate.py --quick: 0 failed before each commit.
- Each change is one commit with its own prerelease bump.

AC#5 (old vs fix):
- OLD alpha.398 (vanish.py; t1192c-OLD398-result.json): series order [..., Curve Pos, Current, Markers]. Drawn 4 right after an add and 0 after the next sat/status poll; drawn 0 against list 5 across 4 polls.
- FIX alpha.399: drawn 2 against list 2 across 4 polls.

Not in this task: re-opening the SAT page through Home -> SAT leaves the dashboard charts blank (TASK-1194), and the SAT page start burst refuses the markers GET (TASK-1193).
<!-- SECTION:FINAL_SUMMARY:END -->
