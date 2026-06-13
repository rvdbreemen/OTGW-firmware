---
id: TASK-865.5
title: >-
  feat(rtos): add FreeRTOS frame queue and state-mutex foundation with
  single-writer map
status: To Do
assignee: []
created_date: '2026-06-13 05:47'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.2
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 foundation; thread-safety primitives; depends seq2 = TASK-865.2)
Deliver the two ADR-123 concurrency primitives IN ISOLATION, no behaviour change, before any task/networking phase consumes them: a FreeRTOS queue (parsed-OT-frame producer->consumer) and a single-writer-per-field rule + FreeRTOS mutex for shared OTGWState fields. This task is the provider; seq6 (PIC task) is the first consumer.

## Producer/consumer cut
processOT() also publishes inline (sendMQTTData etc, OTGW-Core.ino:4224/4253/4260/4267) -> do NOT run it in a task. Phase-1 cut: producer enqueues the raw 9-char OT line as `struct OTFrameMsg { char line[10]; uint8_t len; bool suppressOutput; }` (POD, trivially_copyable); consumer dequeues + calls processOT() in loop() context. FreeRTOS queue copies by value (no mutex for frame flow). Both sources converge: PIC handleOTGW (4682)->dispatchOTGWInputLine->processOT(false); OTDirect bridgeFrameToParser (OTDirect.ino:708)->processOT(9,otHideReports). Combo OTDirect-boot has no OTGWSerial: consumer identical, producer differs by boot mode.

## Mutex (a different problem)
Protects the decoded snapshot read cross-task: OTcurrentSystemState (OTGW-Core.h:175), state.otBus.* (OTBustypes.h:23), state.sat.* (SAT serialize). ONE mutex via xSemaphoreCreateMutex() in setup() (ADR-044). Do NOT reuse the _bleSensorsMux spinlock (SATble.ino:127). Provide an RAII OTStateLock helper mirroring MQTTAutoConfigSessionLock (MQTTstuff.ino:75).

## Shims (allowlisted abstraction headers)
platform_esp32.h real impl + platform_esp8266.h compile-safe no-op stubs, dispatched via platform.h: platformMutexCreate/Lock/Unlock, platformQueueCreate/Send/Receive.

## Deliverable: single-writer-per-field map
Grep every writer/reader of OTcurrentSystemState.* / state.otBus.* / state.sat.*; ship a markdown table (owner-writer context + reader contexts) as an ADR-123 Phase-1 appendix. Add a new evaluate.py check: OTGWSerial refs + the OTFrameMsg-enqueue call appear ONLY in the producer region.

## Acceptance Criteria
- build: esp32 + esp32-classic compile clean with the shims; the mutex + OTFrameMsg queue are each created once in setup().
- build: OTFrameMsg is fixed POD with static_assert trivially_copyable; consumer calls processOT() from loop() (not a task); byte-identical OT-log/MQTT output vs pre-change for a captured replay.
- evaluator: new check asserts OTGWSerial/enqueue only in the producer region; evaluate.py --quick no new failures; shims do not raise ESP_ABSTRACTION_BASELINE.
- evaluator: single-writer map committed (markdown table); grep-check no listed single-writer field has a 2nd write site; OTStateLock RAII exists, cross-task writers (processOT) and readers (restAPI.ino) acquire it.
- field: race-freedom under concurrent PIC-task + AsyncTCP load + combo dual-boot frame routing (ESP32-S3 soak, #dev-sat-mqtt; no automated concurrency test exists).
<!-- SECTION:DESCRIPTION:END -->
