---
id: TASK-1321
title: >-
  ot_log_buffer is shared between the loop task and an async REST handler
  without a guard
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T15:26:00Z'
updated_date: '2026-10-23T23:15:00Z'
labels:
  - webui
  - websocket
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 444000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1300 review (pre-existing): sendEventToWebSocket_P (OTGW-Core.ino:126-143) writes the global ot_log_buffer (OTGW-Core.ino:100, 512 B) and is reachable from the async_tcp task (FSexplorer.ino:348 onNotFound -> processAPI -> restAPI.ino:707/714 -> setOTGWSimulationEnabled restAPI.ino:390-392), while the loop task writes the same buffer for every OT log line. A preemption mid-write can mix two lines in one frame. Options: post the event to the loop task (pending flag, consumed on the owner task) or an ADR-090 guard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The async REST path no longer writes ot_log_buffer directly (or the write is guarded), shown by code + a test or bench run
- [x] #2 Simulation start/stop events still reach the WS log
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: the /simulate REST handler (restAPI.ino setOTGWSimulationEnabled) and the telnet 's' key (handleDebug.ino; telnet input runs in AsyncSimpleTelnet's onData on async_tcp) now call requestOTGWSimulationEvent(); handleOTReplay() on the loop task takes the pending value with __atomic_exchange_n and sends the WS event. tests/test_sim_event_single_writer.py (source checks on the real files): HEAD copies 'FAILED (failures=4)', fix OK. Bench (OTGW32 alpha.479+c4447d1 working tree, app OTA): node WebSocket client on /ws while POST /api/v2/simulate/start then stop -> received '* OTGW simulation enabled [replay active]' and 4 s later '* OTGW simulation disabled [live serial resumed]'. Review (opus): fixed A1 telnet twin path + comment no longer claims a single writer, A2 read-then-clear race (atomic exchange). Other async writers (settings POST SAT-SIM rejection, satNarrate from REST) filed as TASK-1337. Behaviour: start+stop within one loop pass coalesce to the last event.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The OTGW simulation start/stop WebSocket event is now written on the loop task only: the REST /simulate handler and the telnet 's' key post it, handleOTReplay() takes it atomically and sends it. Evidence: AC1 source test red on HEAD (4 fails) / green; AC2 bench WS client received both events. Remaining async writers of ot_log_buffer: TASK-1337.
<!-- SECTION:FINAL_SUMMARY:END -->
