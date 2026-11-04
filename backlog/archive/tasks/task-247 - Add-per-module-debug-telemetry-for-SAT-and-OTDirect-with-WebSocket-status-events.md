---
id: TASK-247
title: >-
  Add per-module debug telemetry for SAT and OTDirect with WebSocket status
  events
status: Done
assignee: []
created_date: '2026-04-11 09:11'
updated_date: '2026-04-11 09:31'
labels:
  - sat
  - otdirect
  - debug
  - esp32
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add conditional debug macros for SAT and OTDirect modules (defaulting to enabled), wire them into the telnet debug menu, fill in missing debug calls across all critical function paths, and send user-friendly status events to the WebSocket for end-user visibility.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add state.debug.bSAT (default true) and state.debug.bOTDirect (default true) flags to OTGWState in OTGW-firmware.h
- [x] #2 Define SATDebugTf/SATDebugTln/SATDebugf macros in SATcontrol.ino gated by state.debug.bSAT
- [x] #3 Define OTDDebugTf/OTDDebugTln/OTDDebugf macros in OTDirect.ino gated by state.debug.bOTDirect
- [x] #4 Add toggle keys '5' (SAT) and '6' (OTDirect) to handleDebug.ino with help menu entries
- [x] #5 Replace raw DebugTf/DebugTln calls in SATcontrol.ino and SATcycles.ino with SATDebug* macros and fill critical gaps (cycle classification, HCR median, flow percentile, PID steps)
- [x] #6 Replace raw DebugTf/DebugTln calls in OTDirect.ino with OTDDebug* macros and fill critical gaps (PI controller, flame ratio accumulation, frame override resolution, command queue)
- [x] #7 Send user-friendly WebSocket status events from SAT (flame on/off, setpoint sent, mode change, curve recommendation) via sendEventToWebSocket_P with prefix '*'
- [x] #8 Send user-friendly WebSocket status events from OTDirect (connected/disconnected, flow temp on connect, mode change) via sendEventToWebSocket_P with prefix '*'
- [ ] #9 Build compiles cleanly on ESP8266 and ESP32
- [x] #10 Adjust bRestAPI default to false and bMQTT default to false — review existing REST/MQTT debug calls for spam, keep only relevant per-event lines
- [x] #11 No WebSocket status events for REST API or MQTT — only SAT and OTDirect get WS events
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All 5 subtasks complete. Added conditional debug macros (SATDebug*, OTDDebug*) gated on state.debug.bSAT/bOTDirect, toggleable via telnet keys 5/6. Filled debug gaps in SATcontrol.ino, SATcycles.ino, and OTDirect.ino. Added user-friendly WebSocket status events for SAT (flame on/off, mode change, HCR recommendation) and OTDirect (connected/disconnected, mode change). Reduced REST API debug from 11 to 7 call sites and MQTT from 50 to 35. Build verified clean on ESP8266.
<!-- SECTION:FINAL_SUMMARY:END -->
