---
id: TASK-247.4
title: Add user-friendly SAT and OTDirect status events to WebSocket
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 09:13'
updated_date: '2026-04-11 09:31'
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
- [x] #1 SAT: flame on/off sends 'Heating active, setpoint Xdeg C' / 'Heating off, room Xdeg C'
- [x] #2 SAT: control mode change sends 'SAT mode: <mode name>'
- [x] #3 SAT: heating curve recommendation sends 'Heating curve: gradient X recommended'
- [x] #4 OTDirect: connected sends 'OTDirect connected, flow Xdeg C'
- [x] #5 OTDirect: disconnected sends 'OTDirect disconnected'
- [x] #6 OTDirect: mode change sends 'OTDirect mode: <mode name>'
- [x] #7 Messages are human-readable (no raw hex, no PID internals, no frame numbers)
- [x] #8 Build clean
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added user-friendly WebSocket status events for SAT (SATcycles.ino) and confirmed OTDirect events already added by agent 247.2 (OTDirect.ino). SAT events: flame ON sends 'Heating active, setpoint X.X deg C'; flame OFF sends 'Heating off, room X.X deg C'; HCR INCREASE/DECREASE sends 'Heating curve: X gradient recommended' (HOLD suppressed); mode change sends 'SAT mode: <continuous|pwm|off>'. All messages use sendWebSocketJSON() with snprintf_P into static buffers, type=status. No WebSocket events for REST API or MQTT.
<!-- SECTION:FINAL_SUMMARY:END -->
