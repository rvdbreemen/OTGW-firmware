---
id: TASK-558
title: >-
  Route force-discovery through drip publisher + add maxBlock to throttle
  warnings
status: To Do
assignee: []
created_date: '2026-05-07 11:57'
labels:
  - mqtt
  - heap
  - refactor
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
F-key (`F` in telnet) and `POST /api/v2/otgw/discovery` currently call `doAutoConfigure()` synchronously, which publishes ~386 HA discovery entries in one background-task pass and stalls `handleOTGW()` for ~2.7s (verified in beta.21 log: gap between 11:52:25.745 and 11:52:28.029).

The drip publisher infrastructure (`markAllMQTTConfigPending` + `loopMQTTDiscovery`) already supports the desired behaviour: queue all IDs, publish one per timer tick (2s normal, 10s under heap pressure, deferred during status-frame burst, Dallas pseudo-ID handled). The REST endpoint already returns `202 Accepted` *before* calling `doAutoConfigure()`, so the API contract already implies async.

Replace the body of `doAutoConfigure()` with a call to `markAllMQTTConfigPending()` so both call sites become async. As a paired observability improvement (issue #4 from the heap analysis), also include `ESP.getMaxFreeBlockSize()` alongside `ESP.getFreeHeap()` in the four throttle warning DebugTf log lines — fragmentation visibility without waiting for `logHeapStats`.

This is one of two paired tasks. The 2.0.0 worktree carries the sibling task (`feat-2.0.0: port ...`).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 doAutoConfigure() body in src/OTGW-firmware/MQTTstuff.ino is replaced with a call to markAllMQTTConfigPending() (no other behaviour). Function signature unchanged.
- [ ] #2 Both callers (handleDebug.ino F-key and restAPI.ino POST /api/v2/otgw/discovery) compile and behave correctly without changes to their code.
- [ ] #3 Four throttle warning DebugTf format strings in helperStuff.ino now include maxBlock: HEAP-CRITICAL Blocking WebSocket, WebSocket throttled, HEAP-CRITICAL Blocking MQTT, MQTT throttled. Format goes from (heap=%u bytes) to (heap=%u, maxBlock=%u bytes).
- [ ] #4 ./build.sh exits 0 (firmware + filesystem build clean).
- [ ] #5 python evaluate.py --quick reports no NEW failures versus dev HEAD baseline.
- [ ] #6 Commit message clearly references both parts (drip-route force-discovery; maxBlock in throttle warnings) and the heap-analysis context.
- [ ] #7 After build + evaluator are green, the commit is pushed to origin/dev per CLAUDE.md push policy.
<!-- AC:END -->
