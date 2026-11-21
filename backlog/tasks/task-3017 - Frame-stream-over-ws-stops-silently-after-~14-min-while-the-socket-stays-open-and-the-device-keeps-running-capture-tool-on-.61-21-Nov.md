---
id: TASK-3017
title: >-
  Frame stream over /ws stops silently after ~14 min while the socket stays open
  and the device keeps running (capture tool on .61, 21 Nov)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-21T16:41:00Z'
updated_date: '2026-11-21T18:14:00Z'
labels:
  - tooling
  - bug
dependencies: []
priority: high
ordinal: 1131000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-3006 stream watch (09, .61 beta.413+da42aca, capture tool 1c16a65 on the NAS): last frame 16:51:44 (device 16:51:42.734), the /ws connection stayed open until 'disconnected from /ws' 17:38:20, the device kept its OT traffic (telnet processOT ~720 lines/min 16:38-17:38) and reported WS_drops=0 every minute; no WS line on the device around 16:51:4x. Either the firmware stops sending to this client or the tool's WS reader stalls. Possibly the same as will_85903's 08:04 silence on 20 Nov (both device streams). Evidence: OTGW-field-data/bench-otgw32/2026-11-21_task3006-capture-1c16a65/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced: a capture on .61 shows the /ws frame stall again, with a device-side and a tool-side trace that tell which side stops
- [ ] #2 Root cause fixed with a test that fails on the old code
- [ ] #3 A capture of >= 2 h on .61 has no /ws silence while the device runs
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Fix plan (after TASK-3011): (a) count every message textAll() did not enqueue for all clients: DISCARDED and PARTIALLY_ENQUEUED add to iWsDropsTotal (and a per-reason counter for the stalled-client case), so ws_drops stops reporting 0 while a client gets nothing; (b) close a client whose queue stays full instead of starving it silently: set closeWhenFull on the otLogWs clients (library option, one call per client at connect) or our own check (queueIsFull() for N consecutive sends -> client->close()); prefer the library option if its close path is safe under AsyncTCP (verify in AsyncWebSocket.cpp:469-480: it unlocks before close, so the destructor chain is not under the lock); the browser reconnects on close; (c) host test that reproduces a non-draining client: a fake AsyncWebSocketClient queue that never drains, 33+ messages -> old code: socket open, nothing counted (red); fix: drops counted and the client closed (green); (d) bench by 09: a /ws client that stops reading (paused reader) -> device closes it within the queue limit, ws_drops > 0, a fresh client still gets frames; no restart.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: deferred to 2026-11-25 (token budget), like the rest of this lane.

Bench check 2026-11-21 17:45 (otgw-bench-tester), .61 beta.413+da42aca, no reflash/reboot since 15:42: (1) fresh /ws connection (stdlib client from the NAS): HTTP 101, 296 frames in 30 s, first after 0.29 s, last at 30.09 s -> a new client gets frames. (2) the stalled connection no longer exists: the capture tool closed it at stop ('disconnected from /ws' 17:38:20), not testable. (3) device/info: uptime 2h02, bootcount 1168 (no restart), freeheap 48028, maxfreeblock 31732, hd_min_free_heap 116 B; for comparison .63 (beta.421+464a188, 25 h soak) hd_min_free_heap 20884 B. When the 116 B minimum happened is not visible (capture telnet logHeapStats HEALTHY every minute, WS_drops=0; device state at capture start showed low/warn/crit 13/3/1). Hypothesis, untested: a WS send dropped under heap pressure without WS_drops counting. .61 left as is (no reflash/reboot) to keep the evidence. Evidence: OTGW-field-data/bench-otgw32/2026-11-21_task3006-capture-1c16a65/.

2026-11-21 (otgw-dev, read-only analysis, capture OTGW-field-data/bench-otgw32/2026-11-21_task3006-capture-1c16a65/): (1) Heap is not the trigger: logHeapStats 16:50:39 / 16:51:39 / 16:52:39 = 47368 / 47292 / 43276 B free, max block 31732, HEALTHY (transcript lines 18548/20016/21323), around the 16:51:44 stall. (2) The drop counter cannot see it: ws_drops counts only canSendWebSocket() refusals at heap CRITICAL (helperStuff.ino:1227-1231); sendLogToWebSocket() ignores the return of otLogWs.textAll() (webSocketStuff.ino:289-303). (3) Likely mechanism: ESPAsyncWebServer 3.11.2 AsyncWebSocketClient::_queueMessage() (.pio/libdeps/esp32-combo/ESPAsyncWebServer/src/AsyncWebSocket.cpp:469-487) silently discards every new message once that client's queue holds WS_MAX_QUEUED_MESSAGES = 32 (AsyncWebSocket.h:17), unless closeWhenFull is set; textAll() (:1205-1216) then returns DISCARDED / PARTIALLY_ENQUEUED and nothing counts it. A client whose queue stops draining keeps its socket open and gets nothing, while a fresh client gets frames: matches the three observations. Not yet confirmed on the device (queue length of the stalled client, or the library's 'Too many messages queued' warning). (4) Separate open item: hd_min_free_heap 116 B on .61 was already 144 B / max_block 500 at the capture start (16:38, uptime 0:55), so that dip predates the capture and is not this stall.
<!-- SECTION:NOTES:END -->
