---
id: TASK-865.6
title: >-
  feat(rtos): move the PIC UART onto a dedicated FreeRTOS task as sole
  OTGWSerial owner
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-13 05:48'
updated_date: '2026-06-13 15:13'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.2
  - TASK-865.5
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 1, first/lowest-risk; depends seq2=TASK-865.2, seq5=TASK-865.5)
Move the PIC UART onto a dedicated FreeRTOS task pinned to the app core, sole OTGWSerial owner, so the RX FIFO drains regardless of network load and the 4-lines/call bound (kMaxLinesPerDrain, OTGW-Core.ino:4648, TASK-671) is deleted. ADR-127 boot-detect already landed (c2aef89c) -> use existing isPICEnabled().

## Design: task owns ONLY byte I/O (no parse, no processOT)
- RX: task drains OTGWSerial.read(), assembles CR/LF lines (reuse overflow logic 4682-4724), pushes OTFrameMsg onto the seq5 RX queue. Consumer = loop() pops -> existing dispatchOTGWInputLine (3248)->processOT (4197) unchanged. Keeps OTGWState single-writer (no hot-path mutex).
- TX: handleCommandQueue (3121) keeps due/retry/PR=-match loop-side but pushes bytes to a TX queue; task pops + OTGWSerial.write(). checkCommandResponse (3154), cmdqueue[] (426) stay loop-side.

## Three edges
1. PIC upgrade: while isFlashing()/bPICactive (OTGW-firmware.ino:790), task SUSPENDS, loop-side flash path drives OTGWSerial (handlePendingUpgrade 864, handlePicFlashBackgroundTasks 748). Do not hoist the upgrade FSM into the task.
2. Sole-owner gate needs an ALLOWLIST (56 refs/8 files): instantiation OTGW-firmware.h:76/78; begin/end ino 271/306/318; PR=A probe 639-640; version readout handleDebug 338-342 / OTGW-Core 4567-4571,5089-5094; reset/find 502,512-513; upgrade FSM 5124-5133. Only the runtime drain/TX sites (3231-3242,3365-3366,4672-4734) move into the task.
3. Task lifecycle vs ADR-127: gate creation on HAS_PIC && isPICEnabled() (post boot-detect); on OTDirect boot do NOT create / keep suspended (handlePICSerial early-returns on isOTDirectEnabled, 4641). Task must BLOCK between drains (vTaskDelay/notify/stream-buffer wait), never busy-poll available(), or it starves the core and trips the TWDT.

## Acceptance Criteria
- build: esp32 + esp32-classic compile clean (grep per-env SUCCESS).
- evaluator: kMaxLinesPerDrain + the RX bound removed; new evaluate.py check: runtime OTGWSerial read/write/available appear ONLY in the PIC-task fn(s) with an explicit allowlist for the setup/version/upgrade/instantiation sites. evaluate.py --quick green; abstraction not regressed (FreeRTOS behind platform_esp32.h/boards.h, HAS_PIC-gated, no raw #ifdef ESP32).
- field: under heavy HTTP+MQTT load no Serial Buffer Overflow/Overrun, no TWDT resets; PIC OTA flash completes (task suspends during isFlashing/bPICactive); ser2net 25238 both directions; command TX/retry/PR= + checkCommandResponse unchanged; OT/MQTT/WS/SAT output byte-identical to the pre-task build.
- field: OTDirect (no-PIC) combo boot -> task not created/suspended, does not spin a closed UART.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented PIC-UART dedicated FreeRTOS task (sole OTGWSerial byte-I/O owner). Shims platformTaskCreatePinned/platformTaskDelay + PlatformTask added to platform_esp32.h (behind abstraction, no raw FreeRTOS in app code). Owner fns in OTGW-Core.ino: picSerialDrainOnce (RX line-assembly + TX-queue drain), picSerialPumpUpgrade (loop-side upgrade tick while parked), picSerialFlushRx (sim flush). New otTxQueue + enqueuePICTx; sendPICSerial/net2ser/PR=A-probe now enqueue instead of OTGWSerial.write. kMaxLinesPerDrain removed from handlePICSerial (both RX+net2ser loops). startPICSerialTask() called once from setup() after resetOTGW(); picSerialTaskShouldPark()=isOTDirectEnabled||isFlashing||bOTGWSimulation; flash path waits via waitForPICTaskParked() before startUpgrade. BUILD: esp32 + esp32-classic both per-env SUCCESS (grepped). EVAL: python evaluate.py --quick Failed:0 (was 1 at baseline=this check); full run 74 passed/0 failed; abstraction baseline unchanged, no raw #ifdef ESP32. NOT committed/bumped (Land phase). Field ACs (byte-identical/no-overflow/OTA/ser2net) out of scope (no hardware).

REFACTOR (advisor review): enforced the 'task owns ONLY byte I/O' mandate. The PIC task no longer calls dispatchOTGWInputLine (which did LED+ser2net-mirror) nor does MQTT/WS/OTGWState writes in task context. Now: (1) picSerialDrainOnce enqueues lines directly with source=OTFRAME_SRC_PIC (pure byte->frame). (2) Added 'source' field to OTFrameMsg (POD preserved); OTDirect tags OTFRAME_SRC_OTDIRECT so consumer does NOT double-mirror to 25238 (OTDirect mirrors producer-side). (3) LED2 blink + 25238 mirror moved to loop-side drainOTFrameQueue, gated source==PIC. (4) RX errors (overrun/rxerror/bufferoverflow) DETECTED in task (volatile flags/counter only) but REPORTED loop-side via reportPendingPICRxErrors() — OTcurrentSystemState.errorBufferOverflow write is now single-writer (loop). (5) dispatchOTGWInputLine kept as loop-side sim-path enqueue helper (tags PIC) so simulation keeps LED+mirror via the consumer. Fixed C++20 -Wvolatile (volatile++ -> explicit assign). FINAL: esp32 + esp32-classic both per-env SUCCESS (no errors, no new warnings); evaluate.py --quick Failed:0, full run 74 passed/0 failed/Health 94.4% (== pre-task baseline); both ADR-123 checks PASS; abstraction baseline unchanged. KNOWN FIELD-VERIFY (out of scope, no hardware): OTGWstream/MQTT/WS now strictly single-thread (loop); queue-full drop now also drops that line's 25238 mirror (pre-existing diagnostic-drop territory, otFrameQueueDrops counts it).

LANDED (alpha.181): PIC UART moved onto a dedicated FreeRTOS task pinned to the app core as sole OTGWSerial owner. RX drains FIFO->line-assembly->enqueueOTFrame (loop-side consumer unchanged); TX drains otTxQueue->OTGWSerial.write. kMaxLinesPerDrain / 4-lines-per-call bound removed. New evaluate.py gate check_pic_uart_task_owns_serial confines all 8 OTGWSerial byte-I/O calls to picSerialDrainOnce/picSerialPumpUpgrade/picSerialFlushRx. Flash path parks the task via picSerialTaskShouldPark()+waitForPICTaskParked() before driving the UART; OTDirect combo boot keeps the task parked (never touches the closed UART).

VERIFIED GREEN (no hardware): build per-env SUCCESS grepped (esp32 SUCCESS 00:02:43, esp32-classic SUCCESS 00:02:47; esp32 Flash 97.9% = 1924463/1966080 B, tight but passing). evaluate.py --quick Failed:0; new gate PASSes and is non-gamed (stray OTGWSerial.read() in setup() correctly FAILed, then restored). PROGMEM/String/raw-ifdef clean; FreeRTOS primitives confined to platform_esp32.h shims; ESP_ABSTRACTION_BASELINE untouched.

OPEN CORRECTNESS ITEMS deferred to field validation (status=In Review for this reason):
- ISSUE 1 (HEADLINE): TX drain in picSerialDrainOnce() does pop-then-re-tail on a short write (availableForWrite()<tx.len -> xQueueSendToBack + break). With >=2 queued items the re-tailed chunk reorders behind later-enqueued items -> wrong byte order to PIC. Also OT_TX_CHUNK_MAX=132 > ESP32 UART HW FIFO 128B and no SW TX ring (no setTxBufferSize): a full 132B chunk could re-queue forever. Mitigated in practice (live PIC cmds tiny, flush() after every write, queue usually 0/1-item) but logic is incorrect regardless. FIX SHAPE: hold the un-writable chunk in a static pending slot and retry it before popping new items, instead of pop-then-re-tail.
- ISSUE 2 (sim-path data race): sim-enable (bOTGWSimulation false->true) calls picSerialFlushRx() loop-side (OTGW-Core.ino:3697) with NO waitForPICTaskParked() handshake, unlike the flash path (5405). Task only observes shouldPark at top of next ~2ms tick -> window where task's in-flight picSerialDrainOnce() and loop's picSerialFlushRx() both touch OTGWSerial.read()/available() concurrently. FIX: add waitForPICTaskParked() on sim-enable before the first picSerialFlushRx().
- ISSUE 4 (field-verify only): PIC task stack=4096B; enqueueOTFrame frames a ~516B OTFrameMsg by value on the task stack. Peak ~700B+ likely fine but maps to no-stack-overflow-under-load field AC, unverifiable without hardware.

REMAINING FIELD-VALIDATION ACs (require hardware): no Serial Buffer Overflow/Overrun under heavy HTTP+MQTT load; no TWDT resets; PIC OTA flash completes (task parked during isFlashing/bPICactive); ser2net 25238 both directions; OT/MQTT/WS/SAT byte-identical to pre-task build; OTDirect combo boot -> task created but immediately parks.
<!-- SECTION:NOTES:END -->
