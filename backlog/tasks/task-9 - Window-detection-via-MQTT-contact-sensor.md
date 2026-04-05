---
id: TASK-9
title: Window detection via MQTT contact sensor
status: To Do
assignee: []
created_date: '2026-04-05 10:05'
updated_date: '2026-04-05 10:21'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies:
  - TASK-2
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python detects open windows via contact sensors and switches to Activity preset (lowest temperature) to save energy. On the ESP we receive window status via MQTT subscribe. Implement a simple mechanism: if an MQTT topic indicates a window is open, start a timer. If the window stays open longer than the configurable minimum_open_time, switch to Activity preset. On close: restore previous preset/target temp. Multiple windows supported via a single MQTT topic (OR logic via HA template). Activity preset temp must be configurable (task 2).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New setting settings.sat.bWindowDetection (default false)
- [ ] #2 New setting settings.sat.iWindowMinOpenSec (default 60, range 10-600)
- [ ] #3 New state fields: state.sat.bWindowOpen, state.sat.iWindowOpenSinceMs
- [ ] #4 State tracking: state.sat.fPreWindowTarget (target temp before window open event)
- [ ] #5 State tracking: state.sat.iPreWindowPreset (preset before window open event)
- [ ] #6 MQTT subscribe: set/<nodeId>/sat/window (payload: open/closed or 1/0 or ON/OFF)
- [ ] #7 On window open + timer expired: switch to Activity preset, save previous target/preset
- [ ] #8 On window closed: restore saved target temp and preset
- [ ] #9 REST API: GET /api/v2/sat/status includes window_open and window_detection fields
- [ ] #10 REST API: POST /api/v2/sat/window (manually set open/closed for testing)
- [ ] #11 MQTT publish: sat/window_open (true/false)
- [ ] #12 WebUI: window status indicator in SAT dashboard
- [ ] #13 WebUI: window detection settings (enable, min open time) in settings page
<!-- AC:END -->
