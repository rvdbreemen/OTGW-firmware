---
id: TASK-559
title: >-
  feat-2.0.0: port TASK-558 — Route force-discovery through drip publisher + add
  maxBlock to throttle warnings
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 11:57'
updated_date: '2026-05-07 12:05'
labels:
  - mqtt
  - heap
  - refactor
  - 2.0.0
dependencies: []
ordinal: 23000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of dev TASK-558. Same change shape, applied to the 2.0.0 ESP32+SAT branch.

F-key and `POST /api/v2/otgw/discovery` currently call `doAutoConfigure()` synchronously, blocking `handleOTGW()` while ~386 (or more, with SAT additions) HA discovery entries publish in one background-task pass. The drip publisher infrastructure (`markAllMQTTConfigPending` + `loopMQTTDiscovery`) already supports async behaviour and is present in this worktree at MQTTstuff.ino:1638 (markAll) and the loopMQTTDiscovery section that follows.

Replace the body of `doAutoConfigure()` (MQTTstuff.ino:1958) with a call to `markAllMQTTConfigPending()`. Add `ESP.getMaxFreeBlockSize()` to the four throttle warning DebugTf log lines in helperStuff.ino (:948 / :980 / :1002 / :1034).

ESP32 has more RAM than ESP8266 so the heap-pressure motivation is weaker, but the OT-stall fix remains relevant — and 2.0.0 has more discovery entries due to SAT additions, which makes the synchronous flood worse, not better.

Dev sibling TASK-558 lives in the dev worktree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 doAutoConfigure() body in src/OTGW-firmware/MQTTstuff.ino is replaced with a call to markAllMQTTConfigPending() (no other behaviour). Function signature unchanged.
- [x] #2 Both callers (handleDebug.ino F-key around :352 and restAPI.ino POST /api/v2/otgw/discovery around :569) compile and behave correctly without changes to their code.
- [x] #3 Four throttle warning DebugTf format strings in helperStuff.ino now include maxBlock: HEAP-CRITICAL Blocking WebSocket (:948), WebSocket throttled (:980), HEAP-CRITICAL Blocking MQTT (:1002), MQTT throttled (:1034). Format goes from (heap=%u bytes) to (heap=%u, maxBlock=%u bytes).
- [x] #4 ./build.sh exits 0 (firmware + filesystem build clean for the 2.0.0 target).
- [x] #5 python evaluate.py --quick reports no NEW failures versus feature branch HEAD baseline.
- [x] #6 Commit message clearly references both parts (drip-route force-discovery; maxBlock in throttle warnings), the heap-analysis context, and cross-references dev TASK-558.
- [x] #7 After build + evaluator are green, the commit is pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support per CLAUDE.md push policy.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify heap API in 2.0.0: platformFreeHeap()/platformMaxFreeBlock() abstractions exist (platform_esp32.h/platform_esp8266.h). Use them, mirroring logHeapStats().
2. Verify markAllMQTTConfigPending() covers the same set of entities as the current doAutoConfigure() body (sensors, binary sensors, climate ID 0 incl SAT switches/select via doAutoConfigureMsgid, number ID 27, Dallas pseudo-id). Confirmed.
3. Edit doAutoConfigure() in src/OTGW-firmware/MQTTstuff.ino (~:1958) → replace body with markAllMQTTConfigPending() call (keep settings.mqtt.bEnable guard).
4. Edit four DebugTf format strings in src/OTGW-firmware/helperStuff.ino (~:948, ~:980, ~:1002, ~:1034): add platformMaxFreeBlock() alongside platformFreeHeap().
5. Build: ./build.sh (firmware + filesystem).
6. Evaluator: python evaluate.py --quick (vs feature-branch HEAD baseline).
7. Stage two files, commit with descriptive message, push to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
8. Mark all 7 ACs, write Final Summary, set Done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edited MQTTstuff.ino doAutoConfigure → 8-line function; body replaced with markAllMQTTConfigPending() (kept settings.mqtt.bEnable guard).
Verified markAllMQTTConfigPending() covers all entities the old body published: sensors+binary sensors via 256-bit pending bitmap, climate ID 0 (incl SAT switch/select per TASK-284 piggyback in doAutoConfigureMsgid), number ID 27, Dallas pseudo-id; plus 2.0.0 SAT zone pseudo-id.
Edited 4 DebugTf in helperStuff.ino (~:948, ~:980, ~:1002, ~:1034) to add platformMaxFreeBlock() alongside platformFreeHeap() — used the platform abstraction (mirrors logHeapStats).
Build ./build.sh: exit 0; ESP32-S3 + ESP8266 merged binaries created.
Evaluator python3 evaluate.py --quick: identical to baseline (59 passed / 2 warnings / 0 failed / 7 info; Health 97.1%).
Commit c640e287 pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Routed force-discovery through the drip publisher and instrumented heap-throttle warnings with maxBlock on the 2.0.0 ESP32+SAT branch. Sibling of dev TASK-558.

## Changes

- **`src/OTGW-firmware/MQTTstuff.ino`** — `doAutoConfigure()` reduced from ~85 lines (synchronous publish loop with sensor/binary-sensor/climate/number/SAT switch+select streaming + Dallas tail) to a 3-statement async-enqueue: keep `settings.mqtt.bEnable` guard, then call `markAllMQTTConfigPending()`. The drip publisher (`loopMQTTDiscovery` + `doAutoConfigureMsgid`) already covers the same entity set, with SAT switches/select piggy-backing on climate pseudo-ID 0 (TASK-284) and the 2.0.0-only SAT zone pseudo-id (`OTGWsatzoneid`) handled in `doAutoConfigureMsgid`.
- **`src/OTGW-firmware/helperStuff.ino`** — four throttle-warning `DebugTf` format strings now report `platformMaxFreeBlock()` alongside `platformFreeHeap()`: HEAP-CRITICAL Blocking WebSocket (~:948), WebSocket throttled (~:980), HEAP-CRITICAL Blocking MQTT (~:1002), MQTT throttled (~:1034). Used the platform abstraction mirroring `logHeapStats()` so the same code compiles cleanly on both ESP8266 (`ESP.getMaxFreeBlockSize()`) and ESP32-S3 (`ESP.getMaxAllocHeap()`).

## Why

F-key and `POST /api/v2/otgw/discovery` previously called `doAutoConfigure()` synchronously, blocking `handleOTGW()` while ~386+ HA discovery entries published in one background-task pass. ESP32-S3 has more RAM than ESP8266 so the heap-pressure motivation is weaker on this branch, but the OT-stall fix remains relevant — and 2.0.0 has *more* discovery entries due to SAT additions, which makes the synchronous flood worse, not better. The REST endpoint already returned `202 Accepted` *before* invoking `doAutoConfigure()`, so the API contract was always async; the implementation now matches it.

Separately, the existing throttle warnings only reported `freeHeap`, hiding fragmentation. Adding `maxBlock` makes a fragmented-but-not-empty heap diagnosable at the moment of throttling, which has been a recurring source of "why is heap big but allocations failing" confusion in field reports.

## Tests

- `./build.sh` exit 0; both ESP32-S3 (1.84 MB ino.bin) and ESP8266 (0.80 MB ino.bin) targets built clean.
- `python3 evaluate.py --quick`: 59 passed / 2 warnings / 0 failed / 7 info — identical to feature-branch HEAD baseline. Both warnings are pre-existing and unrelated to this change.

## Risks / follow-ups

None for the doAutoConfigure() change: the drip path was already used in the wild (both at boot and on broker reconnect). This commit just removes the redundant synchronous code path and routes the two manual triggers through the existing async machine.

The `maxBlock` log-line change is purely informational. Format strings stay PROGMEM (`PSTR`).
<!-- SECTION:FINAL_SUMMARY:END -->
