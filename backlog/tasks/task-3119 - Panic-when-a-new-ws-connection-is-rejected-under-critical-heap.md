---
id: TASK-3119
title: Panic when a new /ws connection is rejected under critical heap
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T08:04:00Z'
updated_date: '2026-11-27T13:26:00Z'
labels:
  - bug
  - ws
  - heap
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1174000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the K bench control run 2 on .63 (build 453+e0dde12, so also in beta.452): the unit crashed in task async_tcp 5 s after a heap recovery. Decoded: std::recursive_mutex::lock <- AsyncWebSocketClient::_queueControl (AsyncWebSocket.cpp:446) <- close() (AsyncWebSocket.cpp:540) <- webSocketEvent() webSocketStuff.ino:281 client->close() in the 'Low heap, rejecting connection' branch <- AsyncWebSocket::_newClient (AsyncWebSocket.cpp:1002). Evidence: OTGW-field-data/bench-63/2026-11-27_K/control/crash_run2/ (crashlog sha a9825ad1..., decoded.txt, control elf sha f3b38b4b). The mutex allocation failure under low heap as the cause is an inference, not proven. Owner: otgw-dev. Relayed by the coordinator 2026-11-27.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause shown with file:line (what allocation or state makes the recursive mutex lock fail in the rejecting branch)
- [ ] #2 Host test that calls the real rejecting branch: red on the old code, green on the fix
- [ ] #3 Bench repeat of the 2-tab reconnect sequence of the K control run with no panic on the fixed build (otgw-bench-tester)
- [ ] #4 CHANGELOG known-issue line until fixed, and the line in the next beta notes
- [ ] #5 python evaluate.py --quick shows no new FAIL
- [ ] #6 Follow-up decision recorded: an ESPAsyncWebServer mutex-init check through the ADR-232 patch mechanism (do it or decline, with the reason)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 otgw-dev's analysis and fix plan (his request 09:10, merged into this task instead of a duplicate): bench-tester K-control run 2 on .63, build beta.453+e0dde12, crash 2026-11-27 08:49:16 (first panic only; the second outage was the bench script's K toggle). Decode: OTGW-field-data/bench-63/2026-11-27_K/control/crash_run2/decoded.txt: pthread_mutex_lock <- recursive_mutex::lock <- AsyncWebSocketClient::_queueControl <- close() <- webSocketEvent webSocketStuff.ino:281 (WS_EVT_CONNECT 'Low heap, rejecting connection') <- AsyncWebSocket::_newClient. Hypothesis (otgw-dev): the WS client object is constructed (std::recursive_mutex constructor, pthread_mutex_init under no heap) before our low-heap reject runs. Also reachable in the field (2 tabs reconnecting under low heap). Fix plan: refuse in otLogWs.handleHandshake before the client object exists (heap below HEAP_WARNING_THRESHOLD or client count at MAX_WEBSOCKET_CLIENTS), keep the event branches; host test for the predicate; bench = the same K run plus tabs reconnecting every 5 s, refusals counted.

2026-11-27 otgw-dev: handshake gate implemented locally on lane/ws-reject-lowheap (wt-d3-wsrej). wsAdmitHandshake(freeHeap, connected): refuse when free heap < HEAP_WARNING_THRESHOLD (5120) or connected >= MAX_WEBSOCKET_CLIENTS (3); wsHandshakeHandler registered with otLogWs.handleHandshake() before server.addHandler(); the library answers 401 before the client object exists; WS_BURST_REJECTED_HEAP/MAX counted; the two event branches stay as the second line. Limit: the heap can still fall between the handshake and _newClient() (one round trip). Test tests/test_ws_handshake_heap_gate.py: red before (wsAdmitHandshake not found), green after (6 tests), two mutants red (registration removed, threshold <=). The library crash itself cannot run on the host: bench = K hold + 5 s reconnect tabs. Reboot counter: rebootcount is written by updateRebootCount() (helperStuff.ino:158) from setup() at OTGW-firmware.ino:559, after WiFi/NTP/telnet/mDNS and before startWebserver(); a boot that dies earlier is not counted.

2026-11-27 otgw-dev: .76 K panic (B1 a9aa6a9 and 456, loopTask, std::bad_alloc from operator new in std::deque<AsyncWebSocketMessage>::emplace_back <- AsyncWebSocketClient::_queueMessage <- AsyncWebSocket::textAll <- sendLogToWebSocket): fix on lane lane/ws-send-guard (wt-d3-ring, local): every textAll goes through wsTextAllNoThrow() (try/catch std::bad_alloc, drops the message, counts it in state.heapdiag.iWsAllocDrops = device/info hd_ws_alloc_drops, logs one rate-limited telnet line per 10 s); the log path (valid and sanitised), sendWebSocketJSON and the 30 s keepalive use it, the last two now also count into ws_drops via wsNoteSendStatus. Exceptions are enabled in this build (cpp_flags -fexceptions). Test tests/test_ws_send_nothrow.py (3 tests, REAL functions vs a stub AsyncWebSocket that throws bad_alloc): all 4 send paths return, count 1 alloc drop and 1 ws drop per message, log once per interval; mutant 'exception escapes the helper' red; test_ws_send_status stubs updated (helper pulled into its slice). NOT reachable on the host: the real library/operator new on the unit and whether the library stays consistent after the throw (read: RAII lock, deque strong guarantee). Bench: K hold on .76 x3.

2026-11-27 otgw-dev: lane A (lane/ws-lowmem): the lwIP ring a7237cab2 (prerelease, telnet key T) is part of the measured low-memory block with the cooldown (TASK-3123) and the gate abort; see the TASK-3123 and lane notes.
<!-- SECTION:NOTES:END -->
