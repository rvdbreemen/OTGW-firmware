---
id: TASK-1241
title: >-
  v2: Monitor > Connection requests otdirect/overrides on PIC boards (503 +
  console error)
status: To Do
assignee: []
created_date: '2026-10-17 09:24'
labels: []
dependencies: []
priority: low
ordinal: 379000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Opening Monitor > Connection always calls fetchOtdOvr() (v2.js:181), which GETs /api/v2/otdirect/overrides. On a PIC board that returns 503 'No OT-direct hardware' and logs a console error. otdirect/status is already gated on the board type (v2.js:1397); the overrides call is not. Found in the alpha.427 Classic-S3 bench rerun (.tmp/bench-classic/after427/ui_I.json, load 5). Also counts against the 2-in-flight REST budget for nothing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a PIC board, opening Monitor > Connection makes 0 requests to /api/v2/otdirect/overrides and logs 0 console errors (browser capture on the Classic-S3)
- [ ] #2 On an OT-Direct board the overrides panel still loads (browser capture on the OTGW32)
<!-- AC:END -->
