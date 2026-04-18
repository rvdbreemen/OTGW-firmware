---
id: TASK-293
title: Fix bridgeFrameToParser PS=1 short-circuit freezes OT-direct state pipeline
status: To Do
assignee: []
created_date: '2026-04-18 13:19'
labels:
  - otdirect
  - esp32
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTDirect.ino bridgeFrameToParser() (line 449-454) returns early when otHideReports (PS=1) is true, BEFORE calling processOT(). Since bridgeFrameToParser is the sole entry point feeding OT-direct frames into the parser (~17 call sites across master/handshake/slave/master-mode/loopback paths), PS=1 freezes OTcurrentSystemState, stops MQTT publishes, stalls the WebSocket OT log, disables auto-leave-PS detection (OTGW-Core.ino:3733-3736), and makes emitSummaryLine synthesize CSV from stale state. SAT PID then runs on frozen Tboiler/Toutside/RelModLevel. PIC's real PS=1 only suppresses serial OUTPUT; parsing continues internally. Fix: move suppression from parser entry to per-frame log/emit layer inside processOT(). Found by ultrareview bug_006 on 2026-04-18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bridgeFrameToParser always calls processOT() regardless of otHideReports
- [ ] #2 processOT() gates only AddLog / sendEventToWebSocket / per-frame sendMQTTData(otmessage) on otHideReports or state.otBus.bPSmode
- [ ] #3 OTcurrentSystemState updates, state.otBus flag writes, and decoded MQTT value publishes run unconditionally
- [ ] #4 Auto-leave-PS-mode detection (OTGW-Core.ino:3733) works when PS=1
- [ ] #5 emitSummaryLine produces fresh (non-stale) CSV summaries
- [ ] #6 ESP32 build (OTGW32) still green
<!-- AC:END -->
