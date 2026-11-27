---
id: TASK-3122
title: >-
  loop-task /ws log send: catch std::bad_alloc, drop the line, count it (K panic
  on .76, a9aa6a9/456 stacks)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T12:33:00Z'
updated_date: '2026-11-27T20:03:00Z'
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
- [x] #1 Every textAll()/text() call on the loop task in webSocketStuff.ino sits inside try { ... } catch (const std::bad_alloc&) { ... }, keepalive and log broadcast included (grep the call sites). The catch counts a drop (iWsDropsTotal) and a new counter ws_alloc_fail in GET /api/v2/device/info, and logs at most once per 10 s
- [x] #2 Host test with a stub textAll() that throws std::bad_alloc, calling the real sendLogToWebSocket: no terminate, the counter rises, wsSafeBufBusy is false afterwards; red on the code before the change, and a mutant without the catch fails it
- [ ] #3 A grep note in the task lists other loop-task allocations that can throw (std containers, make_shared, String growth) in MQTTstuff, restAPI and OTGW-Core, with a judgement for each; only the /ws sites are fixed here, the others become their own tasks if needed
- [ ] #4 Bench on the unit without PSRAM (otgw-bench-tester): K x3 with this build, no panic; ws_alloc_fail > 0 in at least one run, otherwise the run is reported as 'path not hit'
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 16:08 otgw-dev: pushed to origin/dev as merge commit f13bca537dcfa588c2e9d020bc44786e45cf384c (ff-only on c1326b990); the fix commit 3f9176298 (githash 3f91762, image 2.0.0-beta.458+3f91762) is its second parent and src/ is identical to it. Fix: every /ws broadcast goes through wsTextAllNoThrow(), which catches std::bad_alloc, drops the message, counts it (hd_ws_alloc_fail in /api/v2/health) and logs once per 10 s. Evidence: tests/test_ws_send_nothrow.py (4 tests, real send paths against a throwing textAll stub, mutants red); bench .76 with K x3 on 3f91762 = 0 panics in 3 (a9aa6a9: 2 of 3), the catch fired 502x in run 2; soaks 61 min on .63 and 71 min on .76 passed (bench-tester verdict files via coordinator). The commit message of 3f9176298 names TASK-3119 only (TASK-3122 files were not on its base). Still exposed, same family (the /ws broadcast path): client->close() in webSocketEvent reject branches, cleanupClients(), closeAll() (control-frame allocation); other loop-task allocations (MQTT publish gated only below 3072 B, String/vector growth, telnet) not audited.

otgw-architect verification 2026-11-27 21:xx (D-class triage): AC1 holds: every /ws send on the loop task goes through wsTextAllNoThrow() (webSocketStuff.ino:383, 425, 493, 498; only textAll at :467 inside try/catch), drops count via wsNoteSendStatus -> iWsDropsTotal (:454), hd_ws_alloc_fail in sendDeviceInfoV2 (restAPI.ino:3544), log once per 10 s (WS_ALLOC_LOG_INTERVAL_MS). AC2 holds: tests/test_ws_send_nothrow.py + test_platform_tcp_abort_where.py 16 passed 1 skipped (rerun 21:xx); mutant red per commit 3f9176298 message. OPEN: AC3 (the grep note with a judgement per site in MQTTstuff/restAPI/OTGW-Core is not written; the 16:08 note says 'not audited'). AC4 is a bench AC for otgw-bench-tester: evidence OTGW-field-data/_train/b1rel61b.verdict.txt:3 'K x3 on this build BEFORE the soak ... 0 panics in 3 runs' and b1rel2.verdict.txt:13 'hd_ws_alloc_fail +502 in run 2'.
<!-- SECTION:NOTES:END -->
