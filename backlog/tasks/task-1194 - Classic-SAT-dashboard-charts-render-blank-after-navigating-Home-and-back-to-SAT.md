---
id: TASK-1194
title: >-
  Classic SAT dashboard charts render blank after navigating Home and back to
  SAT
status: Done
assignee:
  - '@claude'
created_date: '2026-10-02 08:55'
updated_date: '2026-10-02 18:42'
labels:
  - bug
  - webui
  - sat
dependencies: []
priority: medium
ordinal: 316000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-02 on the bench (OTGW32, alpha.398, classic UI, Playwright on headless Edge) while verifying TASK-1192. A fresh page load on /#sat renders the Temperature History and Heating Curve charts. After the top navigation Home -> SAT, both chart areas stay white: document.elementFromPoint() at the heating-curve grid returns DIV#sat-curve-chart instead of its CANVAS, so mouse clicks never reach zrender and nothing can be added or inspected on the curve. The ECharts instance id stays the same across the re-open (echarts.init() on the same container returns the existing instance, with a warning), which suggests the charts were sized while the SAT page was hidden (display none) and are not resized or redrawn when it is shown again. Screenshots and probes: %LOCALAPPDATA%/OTGW-capture/task1172-bench-20261002/ (overlay-open2.png and the clickdebug/overlay scripts are in the session scratchpad and will be copied there). Related: TASK-812 handled a resize on the SAT settings page, not the dashboard; TASK-1192 (curve click and markers), TASK-1193 (SAT page start burst).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause identified from evidence in the browser (instance and canvas sizes before and after the re-open, or the init/resize sequence)
- [x] #2 Old-vs-fix on the bench with a Playwright/CDP capture: after Home -> SAT five times, both SAT charts render each time and a click on the heating-curve grid reaches the canvas (elementFromPoint returns the CANVAS)
- [x] #3 build.bat for esp32-combo prints its SUCCESS line for firmware and filesystem; python evaluate.py --quick shows no new failures; the change lands in one commit with its own prerelease bump
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-02 root cause from the browser (blankprobe.py, alpha.399):
- Fresh /#sat: both chart containers hold their ECharts DOM, one canvas each.
- After Home -> SAT: both containers have 0 children and 0 canvases. The ECharts instances still exist with the same ids, not disposed, with their old sizes.
- sat.js start() runs initChart() and initCurveChart() on every open. Both first call clearChartUnavailable(container), which set container.textContent = '' unconditionally and so removed the chart DOM. echarts.init() on the same container then returns the existing instance, which keeps painting into its now detached wrapper.

Fix: clearChartUnavailable() only clears when the container shows the 'chart unavailable' placeholder (class chart-unavailable).

Old vs fix through route interception (t1194.py: the patched sat.js served to the device page, firmware unchanged), fresh open plus 5 re-opens through the top navigation:
- OLD: 0 canvases in both charts on every re-open, and elementFromPoint at the curve grid point (5, 50) found no canvas.
- FIX: 1 canvas in each chart on every re-open, and elementFromPoint returned the CANVAS.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
After Home -> SAT, both classic SAT dashboard charts (Temperature History and Heating Curve) stayed white, and clicks on the curve never reached the canvas.

Cause, from the browser (blankprobe.py):
- start() runs initChart() and initCurveChart() on every open. Both first called clearChartUnavailable(container), which set textContent = '' unconditionally and removed the ECharts DOM.
- echarts.init() on the same container then returns the existing instance (same id, not disposed), which kept painting into its detached wrapper.
- After a re-open the containers held 0 children and 0 canvases.

Fix (0d3514335, alpha.401): clearChartUnavailable() only clears when the container shows the 'chart unavailable' placeholder (class chart-unavailable).

Evidence. OTGW32, Playwright on headless Edge, fresh open plus 5 re-opens through the top navigation (t1194.py). Captures in %LOCALAPPDATA%/OTGW-capture/task1194-bench-20261002/.

AC#1: root cause from the instance and canvas state before and after the re-open (task notes).

AC#2 (old vs fix):
- OLD alpha.399: 0 canvases in both charts on every re-open, and elementFromPoint at the curve grid point (5, 50) found no canvas.
- FIX: 1 canvas in each chart on every re-open, and elementFromPoint returned the CANVAS. Measured on the flashed alpha.401 (t1194-DEV401) and before the flash through sat.js route interception (t1194-FIX).
- The alpha.401 screenshot after the fifth re-open shows both charts drawn.

AC#3:
- build.bat --target esp32-combo for alpha.401+0d35143: firmware SUCCESS 147.1 s, filesystem SUCCESS 19.9 s, 'Build completed successfully!'.
- The images were flashed with --update --app --fs, and the fs version.hash 0d35143 matched.
- evaluate.py --quick: 0 failed.
- The change is one commit with its prerelease bump.
<!-- SECTION:FINAL_SUMMARY:END -->
