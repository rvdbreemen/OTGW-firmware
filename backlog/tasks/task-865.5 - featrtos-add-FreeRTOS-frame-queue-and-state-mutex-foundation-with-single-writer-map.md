---
id: TASK-865.5
title: >-
  feat(rtos): add FreeRTOS frame queue and state-mutex foundation with
  single-writer map
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-13 05:47'
updated_date: '2026-06-13 12:34'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented ADR-123 Phase-1 concurrency foundation (no behaviour change, byte-identical by construction).

SHIMS (platform_esp32.h, allowlisted): platformMutexCreate/Lock/Unlock, platformQueueCreate/Send/Receive over FreeRTOS SemaphoreHandle_t/QueueHandle_t, opaque PlatformMutex/PlatformQueue aliases. Dispatched via platform.h (ESP32-only — TASK-865.2 already dropped platform_esp8266.h, so no ESP8266 stub needed/possible; the task's "platform_esp8266.h no-op stubs" clause is obsolete on this branch). Shims add zero raw platform ifdefs in app code; ESP_ABSTRACTION_BASELINE unchanged (0).

QUEUE+CONSUMER (OTGW-Core.{h,ino}): OTFrameMsg POD {char line[MAX_BUFFER_READ]; uint16_t len; bool suppressOutput;} + static_assert is_trivially_copyable. enqueueOTFrame() (producer helper, null-terminates), drainOTFrameQueue() (consumer, calls processOT under OTStateLock, in loop() AFTER doBackgroundTasks() — never inside it). setupOTConcurrency() creates queue(depth 16)+mutex ONCE, called once from setup() (ADR-044). Producers rewired: dispatchOTGWInputLine (PIC)->enqueue(false); bridgeFrameToParser (OTDirect)->enqueue(otHideReports).

MUTEX (OTStateLock RAII, mirrors MQTTAutoConfigSessionLock): non-recursive; writer side in drainOTFrameQueue, reader side in restAPI.ino sendOTmonitorV2(). Did NOT reuse _bleSensorsMux. Single processOT call chain => no self-deadlock.

AC DEVIATION (line[10] -> line[MAX_BUFFER_READ]=512): spec's char line[10] would TRUNCATE PIC lines. Primary-source evidence: processOT() is fed the FULL PIC line (banners, '='-echoes, PS=1 comma-summaries >256B) via its else-if branches, all producing observable output. 10-byte item breaks byte-identical. Widened to 512 + uint16_t len + null-terminate (non-OT branches use strstr/strchr/strcasecmp_P). AC text ("POD + static_assert trivially_copyable", "byte-identical") still satisfied. Hoisted MAX_BUFFER_READ from handlePICSerial() to OTGW-Core.h.

PRODUCER-SEAM SCOPE: only the 2 named frame-bus producers enqueue. Four OTDirect command-RESPONSE synthesis sites (otDirectBridgeProcessStatus, stats-line builder, synthesizeResponse, otDirectBridgeProcessPRResponse) keep direct processOT() BY DESIGN (their state updates must stay synchronous with command path; queuing defers to loop-end = behaviour change). evaluate.py check confines the ENQUEUE SYMBOL (not processOT, not OTGWSerial which is firmware-wide).

EVALUATOR: new check_ot_frame_queue_producer_region (ADR-123) asserts enqueueOTFrame confined to {OTGW-Core.ino, OTDirect.ino}, queue+mutex created once, OTStateLock present. PASS. evaluate.py --quick: 0 Failed (baseline 0), 62 Passed (was 61 + new check). BASELINE not raised.

SINGLE-WRITER MAP: docs/audits/2026-06-13-adr-123-phase1-single-writer-map.md. Honest exceptions: OTcurrentSystemState.Toutside (2nd writer = OTDirect OT= cmd handler) and state.otBus.bOnline (PIC vs OTDirect = single-writer-PER-BOOT-MODE per ADR-127). state.sat.* single (loop) writer; BLE fields snapshot under _bleSensorsMux.

BUILD (per-env SUCCESS grepped, build.py masks failures): esp32 SUCCESS (RAM 34.8%, Flash 97.9%), esp32-classic SUCCESS. 0 FAILED in both logs.

NO setup-time enqueue deferral: detectPIC reads OTGWSerial directly (not via dispatch); handlePICSerial only runs post-bSetupComplete via doBackgroundTasks. doAutoConfigure uses feedWatchDog (NOT doBackgroundTasks), so no re-entrant enqueue-without-drain window.

NOT committed/bumped/status-changed (Land phase owns that). Field ACs (byte-identical replay, race-freedom soak) out of scope — no hardware; byte-identical satisfied by construction (FIFO drain-to-empty, no drops, suppressOutput carried).

LOCK-PLACEMENT CORRECTION (post-review): moved OTStateLock from the consumer call site (drainOTFrameQueue) to INSIDE processOT(). Reason: the AC requires "cross-task writers (processOT) acquire it", but processOT has FIVE callers — the queue consumer PLUS four OTDirect command-synthesis sites (OTDirect.ino:643/1558/2112/2124) that call processOT directly. A consumer-site-only lock left those four loop-task writes unprotected — exactly the race seq6's PIC task would create. Lock now inside processOT covers all five uniformly. Removed the consumer-site wrap (non-recursive mutex would self-deadlock if both took it). Verified no processOT callee re-takes OTStateLock (only sendOTmonitorV2 acquires it, and processOT does not call it) — no self-deadlock.

evaluate.py check strengthened: now also asserts (3) processOT() body acquires OTStateLock (writer side), and (4) the OTGWSerial frame-INGEST surface (.available()/.read()) is confined to the producer file OTGW-Core.ino (partial down-payment on the AC's "OTGWSerial refs ... only in producer region"; the full "only the PIC task references OTGWSerial" rule lands in seq6 per ADR-123 Enforcement). Check PASS.

REPORT-ACCURACY corrections: (a) the OTGWSerial-confinement half of the evaluator AC is only PARTIALLY met here (ingest read surface confined; full firmware-wide OTGWSerial confinement deferred to seq6 per ADR-123 §Enforcement line 131). (b) "byte-identical" is precisely: frame FIFO preserved + suppressOutput carried + no drops + no setup-time deferral; intra-loop interleaving of frame-processing vs other subsystems shifts (processOT now runs at end-of-loop, not mid-doBackgroundTasks). Observable-equivalence is the field soak AC, not asserted here.

LANDED (alpha.180): ADR-123 Phase-1 concurrency foundation — FreeRTOS OTFrameMsg queue (POD + static_assert trivially_copyable, depth 16) producer->consumer drained in loop() after doBackgroundTasks(), single OTStateLock RAII mutex created once in setup() (ADR-044), platformMutex*/platformQueue* shims in allowlisted platform_esp32.h (ESP_ABSTRACTION_BASELINE unchanged at 0), single-writer-per-field map shipped as docs/audits/2026-06-13-adr-123-phase1-single-writer-map.md, ADR-129 authored, and new evaluate.py check check_ot_frame_queue_producer_region (PASS). Builds: esp32 SUCCESS, esp32-classic SUCCESS (per-env SUCCESS lines grepped). Evaluator: 0 Failed, 62 Passed, 1 pre-existing Warning. AC deviation documented: OTFrameMsg.line is char[MAX_BUFFER_READ]=512 not char[10] (a [10] item truncates full PIC lines and breaks byte-identical; POD/static_assert/null-term preserved). REMAINING field-validation AC (cannot self-certify, status held at In Review): race-freedom under concurrent PIC-task + AsyncTCP load + combo dual-boot frame routing — ESP32-S3 hardware soak in #dev-sat-mqtt. Partial-by-design Phase-1 cuts flagged for seq6: only sendOTmonitorV2() reader acquires OTStateLock (other restAPI snapshot readers tighten in seq6); byte-identical is by-construction-reasoned, not hardware-replayed.
<!-- SECTION:NOTES:END -->
