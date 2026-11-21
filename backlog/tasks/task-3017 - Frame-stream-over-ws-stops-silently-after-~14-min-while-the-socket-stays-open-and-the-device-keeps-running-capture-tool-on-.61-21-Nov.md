---
id: TASK-3017
title: >-
  Frame stream over /ws stops silently after ~14 min while the socket stays open
  and the device keeps running (capture tool on .61, 21 Nov)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T16:41:00Z'
updated_date: '2026-11-21T19:54:00Z'
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

2026-11-21 (otgw-dev) part (a) implemented on lane/d3-wsstall: sendLogToWebSocket() passes every textAll() result to wsNoteSendStatus() (webSocketStuff.ino): DISCARDED or PARTIALLY_ENQUEUED adds 1 to ws_drops (state.heapdiag.iWsDropsTotal; field comment and docs/api/MQTT.md ws_drops row updated: heap CRITICAL refusal OR not queued for every client). Evidence: tests/test_ws_send_status.py compiles the real wsSafeBuf block + wsNoteSendStatus() + sendLogToWebSocket() with the real WsUtf8.h against a stub textAll() returning the library's SendStatus: 5 OK on the fix; on origin/dev webSocketStuff.ino 3 FAIL (40 discarded -> drops 0, partial -> 0, sanitised path -> 0). Related tests 6 files PASS, evaluate --quick 0 FAIL. Part (b), closing a stalled client, NOT done: finding below.

2026-11-21 (otgw-dev) part (b) finding, design question: the library option setCloseClientOnQueueFull(true) (AsyncWebSocket.h:305, used in _queueMessage AsyncWebSocket.cpp:469-480) is not safe with textAll(): AsyncClient::close() calls the discard callback synchronously on the calling task (AsyncTCP.cpp:991-998, _close -> _discard_cb), that callback is AsyncWebSocketClient::_onDisconnect (AsyncWebSocket.cpp:267-269) -> AsyncWebSocket::_handleDisconnect() erases the client from _clients (:1008-1015) under the same std::recursive_mutex (ESPAsyncWebServer.h:262) that textAll() holds while it range-iterates _clients (:1205-1216): the same task re-enters, erases the element being iterated -> invalid iterator, undefined behaviour (likely a crash). Our own 'close the stalled one' check has the same problem from outside the library: getClients() (AsyncWebSocket.h:481) returns the list without its lock, so iterating it on the loop task races the async_tcp task. Options for (b) to decide: A defer (b): ship (a) only; the stall is visible in ws_drops; the web UI/capture tool already reconnect on their own timeouts. B close outside any library iteration: keep our own small id list from WS_EVT_CONNECT/DISCONNECT (critical section), and on a non-ENQUEUED result look each id up with otLogWs.client(id) (locked) and close the TCP of a client whose queueIsFull() via client->client()->close() from the loop task, after textAll() returned (narrow remaining race on the returned pointer). C patch the library (patchpiolibs, as ADR-232 does for AsyncTCP) so textAll() collects full clients and closes them after the loop. Recommendation: A now (small, safe, gives the evidence), then B or C as its own task with a bench test (paused /ws reader) if the stall recurs.

2026-11-21 (coordinator decision): option A for the device side (part (a) only, no library change). Condition checked by code reading (otgw-dev): the v2 web UI does NOT self-heal a socket that stays open and silent. It reconnects only from ws.onclose (src/OTGW-firmware/data/v2.js:2995, scheduleReconnect() 5 s timer :2999-3002); for an open, quiet socket psCheckTick() (:5577-5591) only asks /api/v2/device/time after 15 s of silence to detect PS=1 mode and never closes the socket. So per the decision a client-side watchdog joins this task (otgw-webui, v2.js): while the socket is open, not in PS=1 mode and silent for N s (proposal 60 s; a bench bus without a boiler can be quiet, so N well above the 15 s PS check, and a reconnect is cheap), close it so onclose reconnects. B/C (closing stalled clients on the device) go to a new low task only if the stall recurs.

2026-11-21 otgw-webui, client-side watchdog (coordinator spec): v2.js connectWs wraps onmessage so every message, the 30 s firmware keepalive included, stamps wsLastMsgMs (onWsMessage drops keepalives before wsLastFrameMs, so frames alone would make a quiet bench bus look dead); new wsWatchdogTick(), called first in the 5 s psCheckTick (also in a hidden tab): open socket, not PS=1, no message for WS_SILENCE_MS = 90 s (three missed keepalives; webSocketStuff.ino KEEPALIVE_INTERVAL_MS = 30000) -> ws.close(), onclose reconnects after 5 s. 90 s instead of the proposed 60 s: 60 s is only two keepalive intervals, so one late keepalive would already force a reconnect. Test tests/webui/ws-silence-watchdog.test.mjs (real connectWs + wsWatchdogTick, fake socket and clock; limit >= 2.5x the firmware interval, silent -> close + reconnect, keepalives only -> never closed and still delivered, connecting/PS=1 -> left alone): RED on 14941603d 0/4, GREEN 4/4; poll-spread-hidden 5/0, home-status-states 6/0; evaluate --quick 0 failed. Bench expectation: a paused reader reconnects within about 90-95 s (tick 5 s + reconnect 5 s), ws_drops climbs.

2026-11-21 bench (otgw-bench-tester, beta.416+0bac665 on .61): no restart, crashlog none; fresh client 218 frames/20 s. (A) paused reader: run 2 ws_drops 0 -> 30 within 20 s then flat, run 1 stayed 0. Reading (accepted by the coordinator): a reader that stops ACKing is closed by the library itself through AsyncTCP's ack timeout -> AsyncWebSocketClient::_onTimeout() -> AsyncClient::close() (ESPAsyncWebServer AsyncWebSocket.cpp:274-277, :551-561); afterwards count() (WS_CONNECTED only, :1037-1042) is 0, so nothing is sent or counted; run 1's timeout came before the queue reached 32. Part (a) counts while a connected client's queue is full, as claimed. The slow-but-alive client of the e2e capture (TCP kept alive, WS queue full) stays with TASK-3022. (C) web UI watchdog closed after 150 s and reopened 5 s later in the NAS Chromium tab: accepted (hidden-tab timer throttling: 90 s silence + up to ~60 s); a visible tab is expected at ~90-95 s (not benched). (D) heap low-water 524 B / 503 low heap under that load -> TASK-3024 (high). Evidence OTGW-field-data/bench-otgw32/2026-11-21_task3017-0bac665/.

2026-11-21 (otgw-webui, measured on b03c188ea's v2.js, headless Chrome with a fake socket that opens and stays silent): visible tab -> watchdog close() after 92.1 s (90 s + up to one 5 s tick), then the 5 s scheduleReconnect. Hidden/background tab up to ~150 s because of Chrome's intensive timer throttling (setInterval about once a minute), as on 09's bench. Both heal the socket; no code change.
<!-- SECTION:NOTES:END -->
