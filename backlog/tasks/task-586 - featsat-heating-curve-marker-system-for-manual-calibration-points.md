---
id: TASK-586
title: 'feat(sat): heating curve marker system for manual calibration points'
status: To Do
assignee: []
created_date: '2026-05-08 16:02'
labels:
  - sat
  - ui
  - graph
  - calibration
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 implements a heating curve marker system: users can record calibration points (outside temp, measured flow temp, room setpoint) at known operating conditions. These markers are stored persistently and overlaid on the heating curve graph, allowing manual validation that the configured curve matches real-world boiler behavior. The 2.0.0 SAT Expert page shows the heating curve graph (TASK-579 adds visual distinction for the active curve) but has no calibration marker feature. Add a marker system to the SAT heating curve page.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 User can add a calibration marker by clicking on the heating curve graph at current operating conditions (outside temp + observed flow temp)
- [ ] #2 Markers are stored persistently in LittleFS (e.g., /sat_markers.json), max 20 markers
- [ ] #3 Markers are rendered as distinct dots on the heating curve graph, differentiating them from the active curve line
- [ ] #4 Marker tooltip shows: outside temp, flow temp, date added, optional user label
- [ ] #5 User can delete individual markers from the UI
- [ ] #6 REST API: GET /api/v2/sat/markers returns all markers; POST adds one; DELETE /api/v2/sat/markers/{id} removes one
- [ ] #7 Markers survive firmware upgrade (stored in LittleFS, not firmware flash)
<!-- AC:END -->
