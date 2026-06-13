# ADR-130 PIC-UART Dedicated FreeRTOS Task as Sole OTGWSerial Owner (ADR-123 Phase 1)

## Status

Proposed. Date: 2026-06-13.

This is the **Phase 1 task-lift** that ADR-123 (the 2.0.0 concurrency-model
decision) named as the first phase to land, and the first **consumer** of the
queue + mutex primitives ADR-129 delivered in isolation. ADR-129 §Consequences
states verbatim that "full firmware-wide OTGWSerial confinement ('only the PIC
task may reference OTGWSerial') lands in seq6" - this ADR carries that boundary.

## Status History

status_history:
  - date: 2026-06-13
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-1 PIC-UART task lift delivered by TASK-865.6 on top of the ADR-129 primitives
    changed_via: adr-kit

## Context

ADR-123 (Accepted) chose a **hybrid concurrency model** for the 2.0.0 ESP32-S3
line: a dedicated FreeRTOS task for the PIC UART plus event-driven async
networking. ADR-123 §Rollout made the PIC-UART task **Phase 1**, "the primary
reliability win and the first phase to land," and stated that this phase would
add its own enforceable boundary: "only the PIC task may reference OTGWSerial."
ADR-129 (Proposed) delivered the Phase-1 **foundation** in isolation: the
value-copy OT-frame queue, the `OTGWState` mutex, and the producer/consumer seam,
with **zero intended behaviour change** and no task lift. ADR-129 §Alternative D
deliberately kept the primitives and the task move in separate changes so a
concurrency race would remain bisectable.

This ADR (TASK-865.6) is the task lift ADR-129 anticipated. Before it, the PIC
UART is drained from `handlePICSerial()` on the cooperative loop, bounded to four
lines per call (`kMaxLinesPerDrain`, TASK-671) precisely so a noisy PIC cannot
starve `httpServer.handleClient()`. That four-line cap is a symptom: the reader
shares one thread with everything else, so a burst of HTTP/MQTT work can stall it
into a Serial Buffer Overflow. ADR-123 §Constraints calls the PIC UART
"safety-critical and bursty"; the RX FIFO must never overrun.

Three facts about the **Phase-1 environment** shape the decision:

1. **The networking is still synchronous in Phase 1.** ADR-123's async stack
   (`espMqttClient`, `ESPAsyncWebServer`) is Phase 2 and Phase 3. On this branch
   today the loop still runs `PubSubClient` (synchronous MQTT, including
   ADR-108's accepted 15 s blocking `connect()`) and `httpServer.handleClient()`
   (synchronous web). The PIC task is therefore landing **next to** loop-side code
   that still blocks, not next to a fully off-loaded loop. This constrains the
   task's scheduling priority (see Decision §priority).
2. **PIC control I/O is firmware-wide, not task-local.** OTGWSerial is touched by
   reset/find/version-readout at boot, by the PIC/ESP **flash** state machine
   (ADR-012), by the `PR=A` banner probe (ADR-060), and by command instantiation -
   on ~8 files. Only the **runtime byte I/O** (the RX drain and the TX writes) can
   move onto the task; the control surface must stay where it is and the task must
   yield the UART during a flash.
3. **OTGWState must stay single-writer.** ADR-129 established
   single-writer-per-field for the decoded snapshot. If the task did the parse +
   publish (`processOT()`) it would put MQTT/WebSocket/LittleFS and every
   `OTGWState` write on a second thread, recreating exactly the cross-task
   contention ADR-129 §Alternative A rejected. The task must stay **byte I/O
   only**.

## Decision

Lift the PIC UART runtime byte I/O onto a **dedicated FreeRTOS task pinned to the
application core** (`APP_CPU_NUM`, core 1), making that task the **sole runtime
owner** of `OTGWSerial.read()/.write()/.available()/.availableForWrite()/.flush()`.
The task does byte I/O **only**: no parse, no `processOT()`, no MQTT/WebSocket, no
`OTGWState` write. Everything above the byte layer stays in `loop()` context.

### Task body: byte I/O only

- **RX.** The task (`picSerialDrainOnce()` in `OTGW-Core.ino`) reads bytes,
  assembles CR/LF lines (the same overflow/discard logic the old
  `handlePICSerial()` carried), and pushes each complete line onto the ADR-129
  `otFrameQueue` via `enqueueOTFrame(..., OTFRAME_SRC_PIC)`. The consumer
  (`drainOTFrameQueue()`, loop context) parses it via `processOT()`, unchanged.
  The **four-lines-per-call bound is deleted** (`kMaxLinesPerDrain` gone): the
  task owns its own core slice, so it can fully empty the FIFO each pass without
  starving the web/network stack.
- **TX.** `handleCommandQueue()`, `checkCommandResponse()`, and `cmdqueue[]` stay
  loop-side (the due/retry/`PR=`-match logic is unchanged). The byte write is the
  only part that moves: `sendPICSerial()`, the ser2net (net→serial) relay, and the
  `PR=A` probe now build a small POD chunk and call `enqueuePICTx()`, which queues
  it on a **new** `otTxQueue` (`OTTxMsg`, depth 32); the task pops and writes. The
  `availableForWrite()` flow control moves into the task; a short write re-queues
  the chunk for the next tick to preserve order.

### RX-error split: detect in task, report in loop

UART RX-error signals (HW overrun, framing/parity error, line-buffer overflow) are
**detected** in the task (they require UART access) but only recorded in plain
`volatile` flags/counter. The loop-side `reportPendingPICRxErrors()` (called from
`drainOTFrameQueue()`) does the actual `OTcurrentSystemState.errorBufferOverflow`
write + telnet + MQTT/WebSocket publish, under the ADR-129 `OTStateLock`. This
keeps `OTGWState` single-writer (loop) and the network stack single-threaded. The
read-and-clear of the volatile flags races benignly with the task setting them; for
diagnostic counters that is acceptable, so no atomics are used.

### Side-effects move to the loop-side consumer

The LED2 blink and the ser2net **port 25238** mirror used to run producer-side in
`dispatchOTGWInputLine()`. They move to the loop-side consumer
(`drainOTFrameQueue()`), gated on `msg.source == OTFRAME_SRC_PIC`. A new `source`
field on `OTFrameMsg` tags the producer: OTDirect already mirrors producer-side via
`otDirectBridgeWriteLine()`, so the consumer must **not** mirror an OTDirect frame
again to 25238. The POD remains `std::is_trivially_copyable`.

### Lifecycle and the three edges

1. **Flash coexistence (ADR-012).** `picSerialTaskShouldPark()` returns true while
   a PIC/ESP flash is active (`isFlashing()`/`bPICactive`). The flash FSM calls
   `waitForPICTaskParked()` (a bounded, watchdog-fed spin) before it drives the
   UART, so the task never `read()`s mid-upgrade. While parked, the loop-side
   flash path ticks the OTGWSerial upgrade state machine via
   `picSerialPumpUpgrade()` (a single `available()` poll, which short-circuits to
   `upgradeTick()` while `busy()`). The upgrade FSM is **not** hoisted into the
   task.
2. **Sole-owner allowlist.** Only the runtime drain/TX sites move into the task.
   The boot control I/O, the `PR=A` probe, the version readout, the reset/find,
   the upgrade FSM, and the instantiation remain loop-side; those use OTGWSerial
   **control** methods (`begin`/`end`/`resetPic`/`startUpgrade`/`firmwareVersion`/
   …), which are **not** byte I/O and are out of the sole-owner rule's scope.
3. **OTDirect / boot gating (ADR-127, ADR-060).** The task is created once from
   `setup()` after `resetOTGW()`. `picSerialTaskShouldPark()` gates on
   `isOTDirectEnabled()` - **not** `isPICEnabled()` - so on an OTDirect combo boot
   the task parks immediately and never touches the closed UART, while a PIC build
   recovering from a missed boot ETX still runs the reader during banner recovery
   (ADR-060) when `state.pic.bAvailable` is briefly false. The task **blocks**
   between drains (`platformTaskDelay(2)`), never busy-polls `available()`, so it
   cannot starve the core or trip the Task Watchdog (TWDT).

### Task priority: equal-to-loop, a deliberate Phase-1 refinement

ADR-123 §model-1 specified "a single **high-priority** task." This implementation
creates the task at **priority 1**, which is **equal to** the Arduino `loopTask`
priority on the same core, not above it. This is an intentional refinement of the
ADR-123 wording for the Phase-1 environment, stated openly here:

In Phase 1 the loop still carries synchronous, occasionally blocking work
(PubSubClient MQTT incl. ADR-108's 15 s connect; `httpServer.handleClient()`). A
strictly higher-priority PIC task pinned to the **same** core would preempt and
**starve that loop-side work** before Phases 2 to 3 move networking off-loop. With
equal priority, FreeRTOS round-robin time-slices the two same-priority tasks at the
tick, and the task's `vTaskDelay(2)` yields explicitly each pass - so the FIFO is
drained every few milliseconds regardless of a single loop handler's runtime, which
already removes the four-line cap and the loop-iteration coupling. The "drains
regardless of network load" reliability win in Phase 1 therefore rests on
**time-slicing**, not on preemption. The priority can be raised to a true
high-priority value once Phases 2 to 3 move the blocking networking off the loop;
until then, equal priority is the safer choice and is recorded as a refinement, not
a contradiction, of ADR-123.

### Abstraction (ADR-120)

The dedicated-task machinery is reached only through new platform shims
(`platformTaskCreatePinned`, `platformTaskDelay`, opaque `PlatformTask`) in
`platform_esp32.h`; application code never names `xTaskCreatePinnedToCore` or
`vTaskDelay`. The byte-I/O confinement and the `kMaxLinesPerDrain` removal are
enforced by a new `evaluate.py` gate (see Enforcement). `ESP_ABSTRACTION_BASELINE`
is unchanged; there is no raw `#ifdef ESP32` in application code (the task body and
its statics are `#if HAS_PIC`-gated, a capability flag, not a platform macro).

What this ADR does **not** do: it does not supersede ADR-011/047/048/058/108. Those
are ADR-123 Phases 2 to 4 (async networking + watchdog re-scope); ADR-129 already
recorded that those supersessions are not Phase 1. This ADR is a **sibling of
ADR-129** under ADR-123 Phase 1, and supersedes nothing.

## Alternatives Considered

### Alternative A: Run `processOT()` (parse + publish) on the PIC task

Move the parse onto the task and skip the loop-side consumer hop.

Rejected, for the same reason ADR-129 §Alternative A rejected it: `processOT()`
publishes inline (MQTT, WebSocket, OT-log), touches LittleFS, and writes the whole
`OTGWState` snapshot. Running it on the task would put that entire surface on a task
stack and create cross-task contention on the whole publish path, not just the
decoded snapshot. Keeping the task **byte I/O only** confines the cross-task
hand-off to the small copyable POD ADR-129 already built, and keeps `OTGWState`
single-writer (loop). This is the load-bearing constraint of the whole design.

### Alternative B: Keep the four-lines-per-call cap, just move the drain to the task

Lift the reader onto the task but retain `kMaxLinesPerDrain` inside it.

Rejected: the cap exists only because the cooperative loop shares one thread, so a
long drain would starve `httpServer.handleClient()`. Once the reader is on its own
core slice that starvation cannot happen, so the cap has no purpose and actively
caps throughput under a burst - exactly the overrun ADR-123 wanted gone. Removing
the cap is the point of the lift; ADR-123 §Positive lists it explicitly.

### Alternative C: A true high-priority task on the app core (literal ADR-123 wording)

Create the task above `loopTask` priority on core 1, as ADR-123 §model-1 reads.

Rejected **for Phase 1 only**: in Phase 1 the loop still runs blocking
networking (PubSubClient connect, sync web). A higher-priority task on the same
core would preempt and starve that loop-side work, trading a UART-overrun risk for
a network-stall risk. Equal priority + explicit `vTaskDelay` yield gives the FIFO
its core slice without starving the loop. This is adopted now as a refinement;
the literal high-priority value becomes appropriate once Phases 2 to 3 move
networking off the loop, and can be raised then.

### Alternative D: Keep `OTGWSerial.write()` at each TX call site; only move RX

Move the RX drain onto the task but let `sendPICSerial()`/relay/probe keep writing
the UART directly from the loop.

Rejected: that leaves **two** runtime owners of one UART (the task reads, the loop
writes), which races the UART peripheral and defeats the "sole owner" boundary
ADR-123 §Enforcement named for this phase. A single TX queue with the task as the
only writer keeps the UART access single-threaded and makes the sole-owner rule
checkable by the new CI gate. The cost is a new silent-drop failure mode (below),
accepted as the price of single-ownership.

## Consequences

**Benefits**

- **PIC reliability (the ADR-123 Phase-1 win):** the RX FIFO is drained on its own
  core slice every ~2 ms regardless of a single loop handler's runtime, so a
  burst of HTTP/MQTT work can no longer stall the reader into a Serial Buffer
  Overflow. The four-lines-per-call bound (`kMaxLinesPerDrain`, TASK-671) is
  removed.
- **OTGWState stays single-writer:** the task does byte I/O only; the parse,
  publish, RX-error reporting, LED, and 25238 mirror all run loop-side, so the
  ADR-129 single-writer-per-field discipline holds with no new hot-path mutex.
- **Single UART owner:** all runtime byte I/O is confined to three owner functions
  in one file, enforced by a new CI gate (ADR-080).
- **Abstraction intact:** the task primitives sit behind ADR-120 shims;
  `ESP_ABSTRACTION_BASELINE` is unchanged, no raw `#ifdef ESP32` in app code.
- **Flash and OTDirect coexist cleanly:** the park/unpark handshake lets the
  ADR-012 flash FSM and the ADR-127 OTDirect boot mode keep the UART without the
  task fighting them.

**Trade-offs**

- **"Byte-identical" is precise, not absolute** (the same framing ADR-129 used).
  Two error-path surfaces change deliberately:
  - The Serial-overflow **MQTT cadence** changes from "every 10 overflows"
    (`overflowsSinceLastReport`) to an accumulated count reported per loop drain in
    `reportPendingPICRxErrors()`. The counter value is preserved; the publish
    cadence is not byte-identical.
  - A frame dropped because the OT-frame queue is full also drops that line's 25238
    mirror (the mirror is now consumer-side). This is diagnostic-drop territory,
    counted by `otFrameQueueDrops`.
- **New silent-drop failure mode on TX:** `enqueuePICTx()` returns false on a full
  `otTxQueue`, dropping those bytes (counted in `otTxQueueDrops`). Command-level
  retry (`handleCommandQueue`/`checkCommandResponse`, unchanged) still covers
  commands, but a ser2net relay burst could in principle drop a relayed byte under
  a wedged-TX condition. The old `sendPICSerial()` wrote directly with no queue.
- **New RAM:** the TX queue costs 32 × `sizeof(OTTxMsg)` SRAM on top of ADR-129's
  ~8 KB frame queue. Acceptable on the ESP32-S3; it is real RAM the cooperative
  model did not spend.
- **Phase-1 priority is a refinement, not the literal ADR-123 value** (see
  Decision §priority and Related). The "drains regardless of load" guarantee in
  Phase 1 rests on time-slicing, not preemption, until Phases 2 to 3 land.

**Risks and mitigations**

- *Risk*: the task busy-polls `available()` and starves the core / trips the TWDT.
  *Mitigation*: the task **blocks** between drains via `platformTaskDelay(2)` and a
  20 ms park-idle sleep; it never spins on `available()`.
- *Risk*: the flash FSM and the task both drive the UART during an upgrade.
  *Mitigation*: `bPICactive`/`isFlashing()` flips `picSerialTaskShouldPark()`;
  `waitForPICTaskParked()` (bounded, ~200 ms cap, watchdog-fed) blocks the flash
  start until the task acknowledges it has parked.
- *Risk*: on an OTDirect combo boot the task spins a closed UART.
  *Mitigation*: park gates on `isOTDirectEnabled()`, so the task idles and never
  touches the UART; banner recovery (ADR-060) still runs because the gate is on
  OTDirect, not on `isPICEnabled()`.
- *Risk*: the RX-error volatile read-and-clear races the task's set.
  *Mitigation*: accepted as benign for diagnostic counters; the worst case is one
  delayed or coalesced error report, never a corrupt `OTGWState` (the write is
  loop-side under `OTStateLock`).
- *Risk*: field-only test model cannot bisect a concurrency race.
  *Mitigation*: ADR-129 landed the primitives in isolation first; this task adds
  only the lift on top, keeping the change bisectable as ADR-123 §Risks intended.
  Field ACs (no overflow/no-TWDT under load, OTA flash completes, ser2net both
  directions, byte-identical OT/MQTT/WS/SAT output) are field-validation gates,
  reasoned here, not yet empirically replayed (no hardware in this phase).

## Related Decisions

- **ADR-123 (2.0.0 Concurrency Model)**: parent decision. This ADR delivers its
  **Phase 1** PIC-UART task and carries the Phase-1 "only the PIC task references
  OTGWSerial" boundary ADR-123 §Enforcement deferred. It **refines** ADR-123
  §model-1's "high-priority task" wording to equal-to-loop priority for the
  Phase-1 environment (see Decision §priority). Does **not** supersede ADR-123.
- **ADR-129 (OT-Frame Queue + OTGWState Mutex Foundation)**: this ADR is the first
  **consumer** of ADR-129's queue/mutex/seam; ADR-129 §Consequences named seq6 as
  the place full OTGWSerial confinement lands. Builds on it; does not supersede it.
- **ADR-044 (Single point of instantiation)**: the PIC task is created exactly once
  in `startPICSerialTask()`, called once from `setup()`; the TX queue is created
  once in `setupOTConcurrency()`.
- **ADR-120 (Platform abstraction promoted to library)**: the new
  `platformTaskCreatePinned`/`platformTaskDelay` shims and the `PlatformTask` alias
  live in `platform_esp32.h`; app code never touches raw FreeRTOS task APIs.
- **ADR-127 (Combo ESP32-S3 runtime boot detection)**: the task lifecycle gates on
  the OTDirect-vs-PIC boot mode this ADR established.
- **ADR-060 (PIC availability guard / banner recovery)**: the park gate is on
  `isOTDirectEnabled()` **not** `isPICEnabled()` precisely so banner recovery keeps
  running the reader while `state.pic.bAvailable` is briefly false.
- **ADR-012 (PIC firmware upgrade via Web UI)**: the upgrade FSM stays loop-side;
  the new park handshake (`waitForPICTaskParked`/`picSerialPumpUpgrade`) lets the
  flash path own the UART without the task interfering.
- **ADR-080 (Binding ADR rules must have a CI gate)**: satisfied by the new
  `evaluate.py::check_pic_uart_task_owns_serial` gate.
- **ADR-038 (OpenTherm data-flow pipeline)**: the PIC→processOT fan-out this ADR
  further reshapes (the PIC leg of the pipeline now crosses a task boundary; the
  fan-out below `processOT()` is unchanged).
- **Expected later (NOT this phase):** ADR-108 (Phase 2, espMqttClient), and
  ADR-011/047/048/058 (Phase 4, async networking + TWDT re-scope) per ADR-123
  §Rollout and ADR-129's explicit deferral.

## References

- TASK-865.6 - feat(rtos): move the PIC UART onto a dedicated FreeRTOS task as
  sole OTGWSerial owner (this task; parent EPIC TASK-865, ADR-123 rollout).
- ADR-129 - OT-Frame Queue and OTGWState Mutex Foundation (the Phase-1 primitives
  this consumes).
- `src/OTGW-firmware/OTGW-Core.h` - `OTFrameSource`/`OTFrameMsg.source`, `OTTxMsg`,
  `otTxQueue`, `enqueuePICTx`, `startPICSerialTask`, `picSerialTaskShouldPark`,
  `waitForPICTaskParked` declarations.
- `src/OTGW-firmware/OTGW-Core.ino` - `picSerialDrainOnce` (RX/TX owner),
  `picSerialPumpUpgrade`, `picSerialFlushRx`, `picSerialTaskBody`,
  `startPICSerialTask`, `waitForPICTaskParked`, `reportPendingPICRxErrors`,
  `enqueuePICTx`; the `drainOTFrameQueue` source-gated side-effects; the
  `handlePICSerial()` drain removal and `kMaxLinesPerDrain` deletion;
  `sendPICSerial()` TX-queue re-wire; `fwupgradestart()` park handshake.
- `src/OTGW-firmware/OTGW-firmware.ino` - `startPICSerialTask()` in `setup()`;
  `PR=A` probe via `enqueuePICTx`; `picSerialPumpUpgrade()` in the flash path.
- `src/OTGW-firmware/OTDirect.ino` - `bridgeFrameToParser()` tags
  `OTFRAME_SRC_OTDIRECT` so the consumer does not double-mirror to 25238.
- `src/libraries/Platform/src/platform_esp32.h` - `platformTaskCreatePinned`,
  `platformTaskDelay`, `PlatformTask`.
- `evaluate.py` - `check_pic_uart_task_owns_serial` (the new Phase-1 CI gate).
- Task created at priority 1, pinned to `APP_CPU_NUM`:
  `src/OTGW-firmware/OTGW-Core.ino:729` (`platformTaskCreatePinned(..., 1)`); the
  shim comment that the loop task is also priority 1:
  `src/libraries/Platform/src/platform_esp32.h:461`.
- ESP-IDF FreeRTOS SMP / `xTaskCreatePinnedToCore` (pinning + priority semantics):
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html>
- ESP-IDF Task Watchdog Timer (TWDT) - why a same-core task must yield:
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html>

## Enforcement

Declarative regex (adr-judge) does not fit this rule, which needs Python-level
multi-file confinement, function-span attribution, and call-site counting. The gate
is a code check in `evaluate.py`, not an `adr-judge` `forbid_pattern` block (ADR-080
is satisfied by a named CI gate either way).

`evaluate.py::check_pic_uart_task_owns_serial` asserts, on every run:

1. Every **runtime** OTGWSerial byte-I/O call
   (`.read`/`.write`/`.available`/`.availableForWrite`/`.flush`/`.peek`) appears
   **only** inside the three owner functions in `OTGW-Core.ino`
   (`picSerialDrainOnce`, `picSerialPumpUpgrade`, `picSerialFlushRx`), attributed by
   function-body byte span. A byte-I/O call anywhere else means a second UART owner
   slipped in. Control methods (`begin`/`end`/`resetPic`/`startUpgrade`/
   `firmwareVersion`/…) are out of scope by construction (the regex matches byte-I/O
   methods only), which is the allowlist for the setup/version/upgrade/instantiation
   sites.
2. `kMaxLinesPerDrain` is gone from `handlePICSerial()` (the actual cap, comments
   stripped first), so the four-lines-per-call RX bound is removed.
3. The task is created via `platformTaskCreatePinned()` in `startPICSerialTask()`,
   which is called **exactly once** from `setup()` (ADR-044).
4. `picSerialTaskShouldPark()` gates on `isOTDirectEnabled()` (the ADR-060
   banner-recovery invariant), not `isPICEnabled()`.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
