---
id: TASK-1321
title: >-
  ot_log_buffer is shared between the loop task and an async REST handler
  without a guard
status: To Do
assignee: []
created_date: '2026-10-23T15:26:00Z'
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
- [ ] #1 The async REST path no longer writes ot_log_buffer directly (or the write is guarded), shown by code + a test or bench run
- [ ] #2 Simulation start/stop events still reach the WS log
<!-- AC:END -->
