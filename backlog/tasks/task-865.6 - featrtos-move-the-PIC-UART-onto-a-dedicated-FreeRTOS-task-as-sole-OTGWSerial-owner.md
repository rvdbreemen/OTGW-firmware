---
id: TASK-865.6
title: >-
  feat(rtos): move the PIC UART onto a dedicated FreeRTOS task as sole
  OTGWSerial owner
status: To Do
assignee: []
created_date: '2026-06-13 05:48'
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
