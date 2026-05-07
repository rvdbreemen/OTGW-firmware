---
id: TASK-558
title: >-
  Route force-discovery through drip publisher + add maxBlock to throttle
  warnings
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 11:57'
updated_date: '2026-05-07 12:03'
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
- [x] #1 doAutoConfigure() body in src/OTGW-firmware/MQTTstuff.ino is replaced with a call to markAllMQTTConfigPending() (no other behaviour). Function signature unchanged.
- [x] #2 Both callers (handleDebug.ino F-key and restAPI.ino POST /api/v2/otgw/discovery) compile and behave correctly without changes to their code.
- [x] #3 Four throttle warning DebugTf format strings in helperStuff.ino now include maxBlock: HEAP-CRITICAL Blocking WebSocket, WebSocket throttled, HEAP-CRITICAL Blocking MQTT, MQTT throttled. Format goes from (heap=%u bytes) to (heap=%u, maxBlock=%u bytes).
- [x] #4 ./build.sh exits 0 (firmware + filesystem build clean).
- [x] #5 python evaluate.py --quick reports no NEW failures versus dev HEAD baseline.
- [x] #6 Commit message clearly references both parts (drip-route force-discovery; maxBlock in throttle warnings) and the heap-analysis context.
- [x] #7 After build + evaluator are green, the commit is pushed to origin/dev per CLAUDE.md push policy.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit doAutoConfigure() body in src/OTGW-firmware/MQTTstuff.ino (around :1466) → replace with call to markAllMQTTConfigPending().
2. Edit four DebugTf format strings in src/OTGW-firmware/helperStuff.ino (lines ~1000, ~1032, ~1054, ~1086) to add maxBlock=%u alongside heap=%u.
3. Build: ./build.sh (firmware + filesystem).
4. Evaluator: python evaluate.py --quick.
5. Stage changes, commit with descriptive message, push to origin/dev.
6. Mark all ACs checked, write Final Summary, set Done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edited MQTTstuff.ino doAutoConfigure() → 3-line body that calls markAllMQTTConfigPending(); dropped synchronous publish loop, sessionLock, climate/number/Dallas synchronous blocks. The drip publisher (loopMQTTDiscovery) already handles climate (ID 0), number (ID 27), and Dallas (OTGWdallasdataid via configSensors()) — verified in MQTTstuff.ino:1299-1310 and :1416-1421.
Edited 4 DebugTf throttle warnings in helperStuff.ino (HEAP-CRITICAL Blocking WebSocket :1000, WebSocket throttled :1032, HEAP-CRITICAL Blocking MQTT :1054, MQTT throttled :1086) to add ESP.getMaxFreeBlockSize() alongside ESP.getFreeHeap().
Build ./build.sh: exit 0 (firmware 0.70 MB + filesystem 1.98 MB).
Evaluator python3 evaluate.py --quick: 31 passed / 2 warnings / 1 failed — all pre-existing baseline (header guard mqtt_discovery_verify.h, sendMQTTheapdiag buffer regex, 2 unresolved ADR refs); no new failures introduced.
Commit c0e5bb5e pushed to origin/dev (7728aefe..c0e5bb5e). adr-judge: 0 violations, 56 advisory (all llm_judge ADRs, expected).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Routed force-discovery through the existing drip publisher and added heap-fragmentation visibility to throttle warning logs.

## What changed
- `doAutoConfigure()` in `src/OTGW-firmware/MQTTstuff.ino` shrank from a ~70-line synchronous publish loop (sensor + binary sensor + climate + number + Dallas) to a 3-line function that just calls `markAllMQTTConfigPending()`. The function signature is unchanged so both call sites — F-key in `handleDebug.ino` and `POST /api/v2/otgw/discovery` in `restAPI.ino` — work without modification.
- The drip infrastructure (`loopMQTTDiscovery()` + `markAllMQTTConfigPending()`) already covers everything the synchronous body did: climate IDs (0), number ID (27), Dallas pseudo-ID via `configSensors()`, and the heap/PIC/firmware diagnostic IDs. Verified in `MQTTstuff.ino:1299-1310` (queue) and `:1416-1421` (Dallas branch in drain).
- Four `DebugTf` throttle warnings in `src/OTGW-firmware/helperStuff.ino` (HEAP-CRITICAL Blocking WebSocket, WebSocket throttled, HEAP-CRITICAL Blocking MQTT, MQTT throttled) now print `maxBlock=%u` alongside `heap=%u`. Format string changed `(heap=%u bytes)` → `(heap=%u, maxBlock=%u bytes)` and a third printf arg `ESP.getMaxFreeBlockSize()` was added.

## Why
Beta.21 logs show a ~2.7s `handleOTGW()` stall (gap from 11:52:25.745 → 11:52:28.029) when the synchronous discovery burst publishes ~386 entities in one background-task pass. The REST endpoint already returned `202 Accepted` *before* invoking `doAutoConfigure()`, so the API contract already implied async; the implementation just hadn't caught up. Routing through the drip publisher eliminates the stall without any new infrastructure.

For the throttle warnings: fragmentation is a heap-pressure failure mode invisible from `freeHeap` alone. `logHeapStats()` runs once per minute, which is far too coarse to correlate with a throttle event. Including `maxBlock` at the warning site makes the diagnosis immediate.

## User impact
- F-key and `POST /api/v2/otgw/discovery` no longer block OT message handling for ~2.7s.
- Discovery now publishes one entity per drip tick (2s normal cadence, 10s under heap pressure), so the broker receives the same 386 entries spread over minutes instead of a single sub-3s burst.
- Throttle warning lines in the telnet debug stream now show fragmentation data at the moment of throttling.

## Tests run
- `./build.sh` — exit 0, firmware 0.70 MB + filesystem 1.98 MB (beta.23).
- `python3 evaluate.py --quick` — 31 passed / 2 warnings / 1 failed, all pre-existing baseline (no new findings introduced by these edits).
- adr-judge pre-commit: 0 violations, 56 advisory (all llm_judge ADRs needing in-session review, expected).

## Risks / follow-ups
- Behavioural change: the force-discovery is now async. Any external workflow that relied on `doAutoConfigure()` being complete by the time the function returns is now incorrect. The REST endpoint already returned `202` before calling it, so HTTP clients are unaffected; the F-key path in telnet is fire-and-forget. No known caller depends on the old synchronous semantics.
- The `MQTTAutoConfigSessionLock` was removed because the drip publisher serialises naturally (one entity per timer tick). If a future feature reintroduces a synchronous publish path, the lock can come back with it.

Closes TASK-558. Sibling task on the 2.0.0 feature branch is TASK-559 (handled by a separate agent in the parallel worktree).
<!-- SECTION:FINAL_SUMMARY:END -->
