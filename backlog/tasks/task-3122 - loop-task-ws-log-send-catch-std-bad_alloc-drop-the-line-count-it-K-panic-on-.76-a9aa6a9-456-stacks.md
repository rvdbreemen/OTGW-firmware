---
id: TASK-3122
title: >-
  loop-task /ws log send: catch std::bad_alloc, drop the line, count it (K panic
  on .76, a9aa6a9/456 stacks)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T12:33:00Z'
labels:
  - webui
  - heap
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1177000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 456 K panic on the unit without PSRAM was an uncaught std::bad_alloc from operator new in deque<AsyncWebSocketMessage>::emplace_back (ESPAsyncWebServer 3.11.2 _queueMessage), reached through textAll() from sendLogToWebSocket (webSocketStuff.ino:469) on the loop task. canSendWebSocket() only sees the total free heap, which was over 3072. The heap was fragmented (dip snapshot 13:19:09: free 3892, max_block 484). Evidence: OTGW-field-data/bench-61/2026-11-27_b1k/, OTGW-field-data/bench-61/2026-11-27_456/crash_run1/decoded.txt, decode note OTGW-field-data/_comms/decode_456_76_k_panic_2026-11-27.md. Draft by otgw-architect: OTGW-field-data/_comms/task_draft_ws_cooldown_and_badalloc_2026-11-27.md. Bench basis: the unit without PSRAM (.76), K runs with emulated browser tabs. Owner: otgw-dev. Not verified: other loop-task allocations that can throw (see AC3).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every textAll()/text() call on the loop task in webSocketStuff.ino sits inside try { ... } catch (const std::bad_alloc&) { ... }, keepalive and log broadcast included (grep the call sites). The catch counts a drop (iWsDropsTotal) and a new counter ws_alloc_fail in GET /api/v2/device/info, and logs at most once per 10 s
- [ ] #2 Host test with a stub textAll() that throws std::bad_alloc, calling the real sendLogToWebSocket: no terminate, the counter rises, wsSafeBufBusy is false afterwards; red on the code before the change, and a mutant without the catch fails it
- [ ] #3 A grep note in the task lists other loop-task allocations that can throw (std containers, make_shared, String growth) in MQTTstuff, restAPI and OTGW-Core, with a judgement for each; only the /ws sites are fixed here, the others become their own tasks if needed
- [ ] #4 Bench on the unit without PSRAM (otgw-bench-tester): K x3 with this build, no panic; ws_alloc_fail > 0 in at least one run, otherwise the run is reported as 'path not hit'
<!-- AC:END -->
