---
id: TASK-1337
title: 'Remaining async_tcp writers of ot_log_buffer: settings POST and SAT narration'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T23:04:00Z'
updated_date: '2026-10-23T23:42:00Z'
labels:
  - websocket
  - concurrency
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 460000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1321 review. After TASK-1321 the OTGW simulation event is posted to the loop task, but other paths still write ot_log_buffer from the async_tcp task while the loop writes OT log lines: (1) generic settings POST restAPI.ino ~4278 updateSetting(SATsimulation=true with a boiler present) -> settingStuff.ino ~1069 sendEventToWebSocket_P('!', 'SAT-SIM: enable rejected...'); (2) satNarrate* (SATcontrol.ino ~29-46, sendEventToWebSocket) reachable from REST via satHandleControlMode (restAPI.ino ~1518/1610 -> SATcontrol.ino ~2329), satSimInjectEvent (restAPI.ino ~1340 -> SATcontrol.ino ~4103-4161) and satClearSafetyTrip (settingStuff.ino ~992 -> SATcontrol.ino ~2020). Same race as TASK-1321: a preemption mid-write can mix two lines in one WS frame. Fix with the same post-and-consume pattern (a small queue for narration lines) or an ADR-090 guard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No async_tcp path writes ot_log_buffer directly; each listed path posts to the loop task or is guarded (code + test that fails on the current code)
- [x] #2 The SAT narration and the SAT-SIM rejection still reach the WS log
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: satNarrate_P/satNarratef_P and the settings SAT-SIM rejection post into a 4-entry ring (portMUX spinlock, no heap/String, drop-oldest with a counted 'N log lines dropped' note); satWsEventDrain() at the top of satControlLoop() (loop task) is the only sender. tests/test_sat_ws_event_ring.py: HEAD copies 'FAILED (failures=4, errors=1)', fix 'Ran 5 tests OK' incl. a host test compiling the real ring code (FIFO, drop-oldest, truncation, 2 producers x 200k posts vs 1 consumer: bad=0). First build FAILED: (1) the Arduino prototype generator put a prototype of satWsEventTake(SatWsEvent&, ...) above the struct -> signature changed to plain char/char*; (2) two DebugTf literals had a raw CRLF inside (edit mangling) -> fixed; the host test did not compile those functions, only the build caught it. Rebuild SUCCESS; bench (OTGW32, app OTA): WS client while POST /api/v2/sat/mode/pwm and /continuous -> '01:41:52 S Control mode: PWM' and '01:41:55 S Control mode: Continuous' received; no crashlog.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT narration and the SAT-SIM rejection no longer write ot_log_buffer from the async_tcp task: they post to a small spinlock-guarded ring that the loop task drains. Evidence: AC1 source + host ring test red on HEAD / green; AC2 bench WS client received the SAT mode lines triggered over REST. Not covered: portMUX itself (host uses a mutex).
<!-- SECTION:FINAL_SUMMARY:END -->
