---
id: TASK-3119
title: Panic when a new /ws connection is rejected under critical heap
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T08:04:00Z'
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
<!-- AC:END -->
