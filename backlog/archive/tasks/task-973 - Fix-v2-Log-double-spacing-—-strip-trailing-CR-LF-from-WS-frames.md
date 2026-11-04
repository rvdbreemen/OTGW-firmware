---
id: TASK-973
title: Fix v2 Log double-spacing — strip trailing CR/LF from WS frames
status: Done
assignee: []
created_date: '2026-07-01 05:29'
updated_date: '2026-07-01 05:30'
labels: []
dependencies: []
ordinal: 185000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
v2 Monitor > Log rendered a blank line between every frame (double-spaced). Cause: WS frames arrive with a trailing '\r\n' (confirmed: frame tail = 'W--]\r\n'), pushLog stored the line as-is, and renderLog's lines.join('\n') then added another newline -> frame's own \r\n + join \n = a blank line between frames. Fix: strip trailing [\r\n]+ in pushLog before storing (also cleans the stream-to-file output). Follow-up to TASK-957/TASK-970.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 Log renders one frame per line, no blank line between frames (dense)
- [x] #2 Verified on .39: monLog has 0 blank-line gaps between frames, 0 console errors
<!-- AC:END -->
