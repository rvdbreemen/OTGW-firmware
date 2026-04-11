---
id: TASK-247.4
title: Add user-friendly SAT and OTDirect status events to WebSocket
status: To Do
assignee: []
created_date: '2026-04-11 09:13'
labels:
  - sat
  - otdirect
  - websocket
  - esp32
dependencies: []
parent_task_id: TASK-247
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Send concise, non-technical status messages to the OT Monitor WebSocket for SAT and OTDirect state changes. Use sendEventToWebSocket_P with prefix '*'. These are visible in the live log viewer in the Web UI. REST API and MQTT modules do NOT get WebSocket events.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT: flame on/off sends 'Heating active, setpoint Xdeg C' / 'Heating off, room Xdeg C'
- [ ] #2 SAT: control mode change sends 'SAT mode: <mode name>'
- [ ] #3 SAT: heating curve recommendation sends 'Heating curve: gradient X recommended'
- [ ] #4 OTDirect: connected sends 'OTDirect connected, flow Xdeg C'
- [ ] #5 OTDirect: disconnected sends 'OTDirect disconnected'
- [ ] #6 OTDirect: mode change sends 'OTDirect mode: <mode name>'
- [ ] #7 Messages are human-readable (no raw hex, no PID internals, no frame numbers)
- [ ] #8 Build clean
<!-- AC:END -->
