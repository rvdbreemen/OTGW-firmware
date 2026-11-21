---
id: TASK-3022
title: >-
  Close stalled /ws clients safely on the device (a client whose send queue
  stays full)
status: To Do
assignee: []
created_date: '2026-11-21T19:03:00Z'
labels:
  - websocket
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1136000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-3017 (part B/C), only if the /ws stall recurs after TASK-3017 (ws_drops counting + web UI watchdog). ESPAsyncWebServer 3.11.2 silently discards messages for a client whose queue holds WS_MAX_QUEUED_MESSAGES (AsyncWebSocket.cpp:469-487). The library's setCloseClientOnQueueFull(true) is unsafe with textAll(): AsyncClient::close() runs the discard callback synchronously (AsyncTCP.cpp:991-998) -> _onDisconnect -> _handleDisconnect erases the client (AsyncWebSocket.cpp:1008-1015) under the recursive mutex textAll() holds while range-iterating _clients (:1205-1216): same-task re-entry erases the element being iterated (undefined behaviour). getClients() is unlocked, so our own iteration races the async_tcp task. Options: B own client-id list from CONNECT/DISCONNECT events + otLogWs.client(id) lookup after textAll() returns, TCP-close a client whose queueIsFull(); C a build-time library patch (patchpiolibs, as ADR-232) so textAll() collects full clients and closes them after its loop.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Chosen option (B or C) recorded with the re-entrancy analysis; a library patch gets an ADR
- [ ] #2 Host test reproduces a non-draining client and shows it closed without touching the client list during iteration
- [ ] #3 Bench: a paused /ws reader on .61 is closed by the device within the queue limit, no restart, a fresh client keeps receiving
<!-- AC:END -->
