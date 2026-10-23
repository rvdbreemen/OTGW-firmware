---
id: TASK-1337
title: 'Remaining async_tcp writers of ot_log_buffer: settings POST and SAT narration'
status: To Do
assignee: []
created_date: '2026-10-23T23:04:00Z'
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
- [ ] #1 No async_tcp path writes ot_log_buffer directly; each listed path posts to the loop task or is guarded (code + test that fails on the current code)
- [ ] #2 The SAT narration and the SAT-SIM rejection still reach the WS log
<!-- AC:END -->
