---
id: TASK-1533
title: >-
  Monitor log: with auto-scroll off the lines still roll past the scrolled-to
  position
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T20:33:00Z'
updated_date: '2026-10-31T21:17:00Z'
labels:
  - webui
  - needs-maintainer-ok
dependencies: []
priority: medium
ordinal: 689000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762, #alpha-testing 2026-10-31 18:54 UTC (msg 1566163576119500821), on alpha.577: with auto-scroll off the view keeps its scroll position but the lines keep rolling by, so you cannot read back. Confirmed in code: renderLog() (data/v2.js:971-1004) rebuilds the newest LOG_SHOW lines on every frame and only skips the scrollTop jump when logAutoScroll is false, so the window slides under a fixed scroll offset. Expected: auto-scroll off keeps the shown lines in place (new lines are buffered, as with Pause) until auto-scroll is turned on again or the user scrolls to the bottom.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With auto-scroll off, the lines on screen stay the same while new lines arrive (browser test against the real v2.js: red on the old page, green on the new)
- [x] #2 Turning auto-scroll on again (or scrolling to the bottom) shows the newest lines
- [x] #3 Bench unit serves the new page
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Correction: Pause does not buffer. pushLog() (v2.js:957) returns early while logPaused, so lines that arrive during a pause never reach logBuf (the stream-to-file capture still gets them). The fix for auto-scroll off should keep the lines in logBuf and only stop the redraw (or redraw the same window), so nothing is lost.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Monitor log: with auto-scroll off the rAF render only updates the counters (renderLogCounts), so the shown lines stay in place; new lines keep going into logBuf. setLogAutoScroll(on) (chip handler) re-renders at once when switched on. data/v2.js.
Evidence: AC1/AC2 tests/webui/log-autoscroll-off-freezes.test.mjs runs the real pushLog/renderLog cut from v2.js: new 2/2 pass; old v2.js (git show HEAD~) fails 'auto-scroll off: the shown lines stay put' (view moved). log-stream-http-fallback 4/4 still pass. AC3 bench .61 alpha.582+443a3c3: served /v2.js contains function setLogAutoScroll( (bench_check PASS). run_tests 80/80, evaluate 0 FAIL.
Not verified: scrolling in a real browser on the device (the test records scrollTop only). Picked up after Robert's 'gadoor' (label needs-maintainer-ok, reported by maarten_41762).
<!-- SECTION:FINAL_SUMMARY:END -->
