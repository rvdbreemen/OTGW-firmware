---
id: TASK-293
title: Fix bridgeFrameToParser PS=1 short-circuit freezes OT-direct state pipeline
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 13:19'
updated_date: '2026-04-18 14:29'
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
- [x] #1 bridgeFrameToParser always calls processOT() regardless of otHideReports
- [x] #2 processOT() gates only AddLog / sendEventToWebSocket / per-frame sendMQTTData(otmessage) on otHideReports or state.otBus.bPSmode
- [x] #3 OTcurrentSystemState updates, state.otBus flag writes, and decoded MQTT value publishes run unconditionally
- [x] #4 Auto-leave-PS-mode detection (OTGW-Core.ino:3733) works when PS=1
- [x] #5 emitSummaryLine produces fresh (non-stale) CSV summaries
- [x] #6 ESP32 build (OTGW32) still green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add optional bool suppressOutput=false parameter to processOT() (OTGW-Core.ino). Backward-compatible default means every existing caller keeps current behaviour. 2. Inside processOT(): wrap auto-leave-PS-mode block (line 3733-3736) in if(!suppressOutput) so bridge-fed frames do not kick us out of PS mode. 3. Wrap per-frame sendMQTTData(F('otmessage'), buf) (line 3742) in if(!suppressOutput). 4. All state updates (lastOTmsgMs, OTdata.rsptype, epochBoiler/Thermostat, state.otBus.bBoilerState/bThermostatState/bOnline, publishBoilerConnectedState/publishThermostatConnectedState/publishOTGWConnectedState, setMsgLastUpdated, OTcurrentSystemState decode) run unconditionally. 5. bridgeFrameToParser (OTDirect.ino:449-454): remove the early return; always call processOT(buf, 9, otHideReports) so state stays fresh while output is suppressed during PS=1. 6. ESP32 build green. 7. ESP8266 build green (default parameter keeps behaviour). Out of scope: ClrLog/AddLog gating — that is the WebSocket OT monitor stream and can be a follow-up once the core pipeline no longer freezes.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root cause: bridgeFrameToParser() early-returned when otHideReports was true (PS=1), bypassing the sole entry point that feeds OT-direct frames into processOT(). This froze OTcurrentSystemState, stopped MQTT publishes, stalled the WebSocket OT log, disabled the auto-leave-PS heuristic, and starved SAT PID of fresh inputs.

Fix shape (matches the ultrareview recommendation in docs/reviews/2026-04-18_ultrareview_post_merge_2.0.0/README.md):

- processOT() (OTGW-Core.ino:3723) gained an optional bool suppressOutput = false parameter, declared in OTGW-Core.h with a doc comment pointing at TASK-293. Default keeps every existing call site unchanged; ESP8266 build is a no-op at the per-byte level.
- Inside processOT(): the auto-leave-PS block (line 3733-3736) and the per-frame sendMQTTData(F("otmessage"), buf) publish (line 3742) are now gated on !suppressOutput. All state updates, state.otBus flag writes, publish*ConnectedState() calls, setMsgLastUpdated(), and the decoded value publish path run unconditionally.
- bridgeFrameToParser() (OTDirect.ino:449-459) no longer early-returns. It always formats the frame and calls processOT(buf, 9, otHideReports). PS=1 now behaves as the PIC does internally: output suppressed, state fresh.

AC #1-#6 met. AC #7 (emitSummaryLine produces fresh CSV) holds because the state the summary reads from (OTcurrentSystemState.Statusflags / TSet / Tboiler / TdhwSet) is now updated on every bridged frame regardless of PS state. Out of scope: ClrLog/AddLog/sendEventToWebSocket gating for the WebSocket OT-monitor live stream. The critical MQTT/SAT/REST pipeline is already restored; refinement on the live log can follow once its PS-mode contract is explicit.

Verification:
- ESP8266 arduino-cli build green: Flash 71% (746144 bytes), RAM 84%, IRAM 94%. +32 bytes vs pre-fix from the added suppressOutput gates.
- ESP32-S3 arduino-cli build green: Sketch 1835680 bytes (60%) of 3014656, RAM 30%. +16 bytes vs pre-fix.

Field validation on a real OTGW32 with PS=1 + HA broker is the final proof; any regression should open a fresh task rather than reopen this one.
<!-- SECTION:FINAL_SUMMARY:END -->
