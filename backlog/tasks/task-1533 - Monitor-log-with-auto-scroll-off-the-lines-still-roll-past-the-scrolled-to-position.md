---
id: TASK-1533
title: >-
  Monitor log: with auto-scroll off the lines still roll past the scrolled-to
  position
status: To Do
assignee: []
created_date: '2026-10-31T20:33:00Z'
updated_date: '2026-10-31T20:36:00Z'
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
- [ ] #1 With auto-scroll off, the lines on screen stay the same while new lines arrive (browser test against the real v2.js: red on the old page, green on the new)
- [ ] #2 Turning auto-scroll on again (or scrolling to the bottom) shows the newest lines
- [ ] #3 Bench unit serves the new page
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Correction: Pause does not buffer. pushLog() (v2.js:957) returns early while logPaused, so lines that arrive during a pause never reach logBuf (the stream-to-file capture still gets them). The fix for auto-scroll off should keep the lines in logBuf and only stop the redraw (or redraw the same window), so nothing is lost.
<!-- SECTION:NOTES:END -->
