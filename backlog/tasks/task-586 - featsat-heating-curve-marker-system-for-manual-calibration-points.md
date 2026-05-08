---
id: TASK-586
title: 'feat(sat): heating curve marker system for manual calibration points'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:02'
updated_date: '2026-05-08 17:21'
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
- [x] #1 User can add a calibration marker by clicking on the heating curve graph at current operating conditions (outside temp + observed flow temp)
- [x] #2 Markers are stored persistently in LittleFS (e.g., /sat_markers.json), max 20 markers
- [x] #3 Markers are rendered as distinct dots on the heating curve graph, differentiating them from the active curve line
- [x] #4 Marker tooltip shows: outside temp, flow temp, date added, optional user label
- [x] #5 User can delete individual markers from the UI
- [x] #6 REST API: GET /api/v2/sat/markers returns all markers; POST adds one; DELETE /api/v2/sat/markers/{id} removes one
- [x] #7 Markers survive firmware upgrade (stored in LittleFS, not firmware flash)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented calibration marker system for the SAT heating curve graph:

Backend (restAPI.ino - already present from previous session):
- GET /api/v2/sat/markers streams /sat_markers.json directly (returns [] when absent)
- POST /api/v2/sat/markers parses outside_temp, flow_temp, label from JSON body; validates bounds; auto-increments id; max 20 markers
- DELETE /api/v2/sat/markers/{id} filters the entry out and rewrites the file

Frontend (data/sat.js):
- loadMarkers() fetches GET /api/v2/sat/markers; parses array (handles both raw array and wrapped object); updates _markers state
- _renderMarkersOnChart() appends a Markers scatter series (purple diamonds, z:9) to the curve chart; filters previous Markers series to avoid duplication; called after loadMarkers() and after each full curve rebuild
- _renderMarkerList() populates #sat-marker-list with marker entries + delete buttons; shows placeholder when empty
- deleteMarker(id) calls DELETE and reloads; addMarkerAtClick(ot,ft) calls POST with auto-label and reloads
- _initCurveClickHandler() registers ECharts click listener on curve chart; on background click only (componentType !== series), converts pixel coords to data coords, clamps to chart bounds, calls addMarkerAtClick()
- start() calls _initCurveClickHandler() and loadMarkers() after initCurveChart()
- deleteMarker exposed in public API
<!-- SECTION:FINAL_SUMMARY:END -->
