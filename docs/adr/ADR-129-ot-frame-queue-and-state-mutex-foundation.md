# ADR-129 OT-Frame Queue and OTGWState Mutex Foundation (ADR-123 Phase 1)

## Status

Proposed. Date: 2026-06-13.

This is the **Phase 1** concurrency-primitive foundation that ADR-123 (the 2.0.0
concurrency-model decision) anticipated. ADR-123 §Enforcement stated "each rollout
phase will add its own enforceable boundaries ... No declarative Enforcement block
until Phase 1." This ADR carries that Phase-1 boundary.

## Status History

status_history:
  - date: 2026-06-13
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-1 concurrency primitives (queue + mutex) delivered in isolation by TASK-865.5
    changed_via: adr-kit

## Context

ADR-123 (Accepted) chose a **hybrid concurrency model** for the 2.0.0 ESP32-S3 line:
a dedicated FreeRTOS task for the PIC UART plus event-driven async networking, with
parsed OT frames crossing task boundaries through a FreeRTOS queue and shared
`OTGWState` fields following a single-writer-per-field rule guarded by a FreeRTOS
mutex. ADR-123 deliberately deferred the concrete code surface: it set out the model
and named the per-phase boundaries it would enforce later.

TASK-865.5 is the **provider** of the two ADR-123 primitives, delivered **in
isolation with no behaviour change**, before any consumer phase exists. The first
consumer is seq6 (the PIC-UART FreeRTOS task, TASK-865.6), which lifts the producer
onto its own task. Delivering the primitives first lets the disruptive task-lift in
seq6 build on an already-disciplined data flow rather than introducing the queue,
the mutex, and the task move all at once.

Two facts shaped the implementation:

1. The frame queue and the state mutex solve **two different problems**. The queue
   moves raw OT lines across the task boundary in FIFO order (it copies by value, so
   the frame flow needs no mutex). The mutex prevents a reader on one task from
   seeing a torn multi-byte field while a writer on another task updates the decoded
   `OTGWState` snapshot.
2. `processOT()` is fed the **full** PIC/OTDirect line, not only 9-char OT frames:
   banners, `=`-command echoes, and PS=1 comma-separated summaries (which can exceed
   256 bytes) all route through its branches and produce observable MQTT / WebSocket
   / OT-log output. The queue item must carry the whole line or byte-identical
   output breaks.

Constraint from the prior phase: TASK-865.2 already removed `platform_esp8266.h`
(2.0.0 is ESP32-S3 only, ADR-128). The platform shims for these primitives therefore
land only in `platform_esp32.h`; the ESP8266 no-op-stub clause in the task body is
obsolete on this branch.

## Decision

Introduce, in isolation, the two ADR-123 Phase-1 concurrency primitives plus the
discipline and the CI gate that protect them.

1. **Value-copy OT-frame queue.** A FreeRTOS queue (`otFrameQueue`, depth 16)
   carries a POD `struct OTFrameMsg { char line[MAX_BUFFER_READ]; uint16_t len; bool
   suppressOutput; }`, asserted `std::is_trivially_copyable` so `xQueueSend` can copy
   it byte-for-byte. The two frame-bus producers (PIC via `dispatchOTGWInputLine()`,
   OTDirect via `bridgeFrameToParser()`) call `enqueueOTFrame()` instead of
   `processOT()` inline. A single consumer, `drainOTFrameQueue()`, dequeues every
   pending frame and calls `processOT()` **from `loop()` context** (after
   `doBackgroundTasks()`, never inside it, since `doAutoConfigure()`'s file-reading
   loop can re-enter `doBackgroundTasks()`). In Phase 1 producer and consumer are
   co-resident in one loop iteration; seq6 lifts the producer onto a task.

   `OTFrameMsg.line` is `MAX_BUFFER_READ` (512), not the `char line[10]` the task
   prose suggested. The wider buffer is load-bearing: a 10-byte item would truncate
   banners, `=`-echoes, and PS=1 summaries and break byte-identical replay. The
   producer null-terminates `line[len]` because `processOT()`'s non-OT branches use
   `strstr`/`strchr`/`strcasecmp_P`, which read to a terminator. `MAX_BUFFER_READ`
   was hoisted from inside `handlePICSerial()` to `OTGW-Core.h` so the read loop and
   the queue item share one source of truth.

2. **OTGWState snapshot mutex + RAII `OTStateLock`.** One FreeRTOS mutex
   (`otStateMutex`, non-recursive) guards the decoded snapshot read cross-task
   (`OTcurrentSystemState.*`, `state.otBus.*`, `state.sat.*`). A RAII `OTStateLock`
   (mirroring `MQTTAutoConfigSessionLock`, ADR-090) takes it on construction and
   releases on scope exit. The **writer** side is acquired **inside `processOT()`**,
   not at the consumer call site, so all five `processOT()` callers are covered
   uniformly: the queue consumer plus the four OTDirect command-response synthesis
   sites that keep calling `processOT()` directly by design. The first **reader**
   site is `sendOTmonitorV2()` in `restAPI.ino`. The lock is non-recursive; a null
   handle degrades to a no-op (unprotected) rather than a deadlock.

3. **Single point of instantiation (ADR-044).** `setupOTConcurrency()` creates the
   queue and the mutex exactly once, called once from `setup()` before any OT path
   can enqueue.

4. **Platform shims (ADR-120).** `platformMutexCreate/Lock/Unlock` and
   `platformQueueCreate/Send/Receive`, over opaque `PlatformMutex`/`PlatformQueue`
   aliases, live in `platform_esp32.h` and dispatch via `platform.h`. Application
   code never touches the raw FreeRTOS API, so `ESP_ABSTRACTION_BASELINE` is
   unchanged (0).

5. **Single-writer-per-field map.** The full writer/reader audit ships as a Phase-1
   appendix (a standalone audit doc, because ADR-123 is Accepted and immutable). It
   documents the two honest exceptions (see Consequences) rather than claiming a
   clean invariant the code does not support.

What this ADR does **not** do: it does not lift any producer onto a task (seq6), and
it does not supersede ADR-047/048/058/108/011. Those supersessions belong to
ADR-123 Phases 2 to 4 when the async networking and the watchdog re-scope land. This
phase is purely additive foundation.

## Alternatives Considered

### Alternative A: Run `processOT()` directly on the PIC task (no queue)

Move the parse onto the FreeRTOS task and skip the producer/consumer queue.

Rejected: `processOT()` publishes inline (MQTT, WebSocket, OT-log) and touches
LittleFS and the HA discovery path. Running it on the task would put all of that on a
task stack and create immediate cross-task contention on the entire publish surface,
not just the decoded snapshot. The value-copy queue keeps the parse + publish in
`loop()` context for Phase 1 and confines the cross-task hand-off to a small,
copyable POD, which is exactly the seam seq6 needs and nothing more.

### Alternative B: Reuse the existing `_bleSensorsMux` spinlock for state protection

`SATble.ino` already has a `portMUX_TYPE _bleSensorsMux` protecting the raw BLE slot
array. Reuse it for `OTGWState` instead of adding a second primitive.

Rejected: a `portMUX` spinlock is an interrupt-disabling critical-section primitive
sized for the very short BLE-callback slot copy; holding it across `processOT()` (or
across a JSON stream on the reader side) would disable interrupts for far too long
and is the wrong tool. The two protect different objects with different hold times. A
dedicated `xSemaphoreCreateMutex()` (blocking, not spinning) is correct for the
longer `OTGWState` critical sections, and keeping the two primitives distinct keeps
their scopes legible.

### Alternative C: Make the queue item a 9-char OT frame (`char line[10]`) as specified

Carry only the parsed 9-char OT frame, per the task prose.

Rejected on primary-source evidence: `processOT()` is fed the full PIC line, not
only OT frames. Banners, `=`-echoes, and PS=1 summaries (>256 bytes) all flow through
its non-OT branches and produce observable output. A 10-byte item would truncate
them and break the byte-identical-output requirement that this no-behaviour-change
task is built around. The item carries `char line[MAX_BUFFER_READ]` instead.

### Alternative D: Do nothing this phase; introduce the queue and mutex inside seq6

Skip the isolation step and add the primitives at the same time as the PIC-task lift.

Rejected: ADR-123 explicitly chose to "land Phase 1 (PIC task) first in isolation to
prove the pattern before networking changes," and concurrency bugs are the hardest
class for this project's field-only test model to catch. Bundling the queue, the
mutex, and the task move into one change would make a race impossible to bisect.
Delivering the primitives with no behaviour change isolates the risk.

## Consequences

**Benefits**

- Provides the exact queue + mutex seam ADR-123 Phase 1 requires, with **zero
  intended behaviour change**, so seq6's PIC-task lift adds only the task, not the
  primitives.
- The cross-task data path is a small copyable POD plus one mutex, both reachable
  only through platform shims (ADR-120) and instantiated once (ADR-044).
- The writer lock lives inside `processOT()`, so adding a future `processOT()` caller
  inherits the protection automatically rather than needing a remembered wrap.
- A new CI gate (`check_ot_frame_queue_producer_region`) keeps the producer seam from
  drifting, satisfying ADR-080 for a pattern-level decision.

**Trade-offs**

- "Byte-identical" is precise, not absolute. It means: frame FIFO order preserved,
  `suppressOutput` carried per item, no drops under the per-poll burst, and no
  setup-time enqueue path that would defer the first frames. It does **not** mean the
  intra-loop interleaving is unchanged: `processOT()` now runs at end-of-loop (after
  `doBackgroundTasks()`) instead of mid-`doBackgroundTasks()`. Observable equivalence
  is the field-soak AC, reasoned here rather than empirically replayed (no hardware).
- The single-writer-per-field rule has two documented exceptions, not a clean
  invariant: `OTcurrentSystemState.Toutside` has a second writer (the OTDirect `OT=`
  command handler), and `state.otBus.bOnline`/`bBoilerState`/`bThermostatState` have
  a PIC writer and an OTDirect writer that are **mutually exclusive per boot mode**
  (ADR-127), i.e. single-writer-per-boot, not single-writer-in-source. seq6 must
  route or lock the `Toutside` `OT=` write once the PIC producer becomes a real task.
- A depth-16 queue of 512-byte items costs ~8 KB SRAM. Acceptable on the ESP32-S3,
  but it is real RAM the cooperative model did not spend.
- OTGWSerial confinement is only **partially** enforced here: the gate confines the
  frame-ingest read surface (`.available()`/`.read()`) to the producer file, but
  OTGWSerial is firmware-wide (reset, flash, command-send, banner-detect), so the
  full "only the PIC task references OTGWSerial" rule lands in seq6.

**Risks and mitigations**

- *Risk*: the lock is non-recursive; a `processOT()` callee that re-takes
  `OTStateLock` would self-deadlock. *Mitigation*: verified that no `processOT()`
  callee acquires it (only `sendOTmonitorV2()` does, and `processOT()` does not call
  it); the consumer-site wrap was removed so the lock is taken at exactly one writer
  layer.
- *Risk*: a full queue would silently lose frames. *Mitigation*: drops are counted in
  a diagnostic counter and surfaced for the field soak; `enqueueOTFrame()` never
  falls back to inline `processOT()` (which would reorder the FIFO). Depth is sized so
  it cannot fill while producer and consumer are co-resident in Phase 1.
- *Risk*: the reader holds `OTStateLock` across the whole `sendOTmonitorV2()` JSON
  stream, which would stall the PIC task once seq6 makes the producer preemptive.
  *Mitigation*: documented in the appendix as a seq6 action item (snapshot under
  lock, then stream outside it, the pattern the BLE path already uses).

## Related Decisions

- **ADR-123 (2.0.0 Concurrency Model)**: parent decision. This ADR delivers its
  Phase-1 primitives and carries the Phase-1 enforcement boundary ADR-123 deferred.
  Does **not** supersede ADR-123.
- **ADR-044 (Global state, single point of instantiation)**: the queue and mutex are
  created once in `setupOTConcurrency()`.
- **ADR-090 (Re-entrancy guard / RAII pattern)**: `OTStateLock` mirrors
  `MQTTAutoConfigSessionLock`.
- **ADR-120 (Platform abstraction promoted to library)**: the `platformMutex*` /
  `platformQueue*` shims live in the Platform library and dispatch via `platform.h`.
- **ADR-127 (Combo ESP32-S3 runtime boot detection)**: explains the
  single-writer-per-boot-mode exception for `state.otBus.bOnline` and friends.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: why the shims are ESP32-only and the
  ESP8266 no-op-stub clause is obsolete.
- **ADR-080 (Binding ADR rules must have a CI gate)**: satisfied by the
  `check_ot_frame_queue_producer_region` gate.
- **ADR-087 (Frame bridge pattern)**: the OTDirect `bridgeFrameToParser()` producer
  re-wired here is the frame bridge's output stage.
- **Consumed by:** seq6 / TASK-865.6 (PIC-UART FreeRTOS task), the first consumer of
  these primitives.

## References

- TASK-865.5 — feat(rtos): FreeRTOS frame queue + state-mutex foundation with
  single-writer map (provider task).
- `docs/audits/2026-06-13-adr-123-phase1-single-writer-map.md` — the ADR-123 Phase-1
  single-writer-per-field appendix (writer/reader map + the two honest exceptions).
  Untracked; must be staged alongside this ADR.
- `src/OTGW-firmware/OTGW-Core.h` — `OTFrameMsg`, `OTStateLock`, `enqueueOTFrame`,
  `drainOTFrameQueue`, `setupOTConcurrency` declarations.
- `src/OTGW-firmware/OTGW-Core.ino` — queue/mutex definitions, `setupOTConcurrency()`,
  `enqueueOTFrame()`, `drainOTFrameQueue()`, `processOT()` writer lock, PIC producer
  re-wire in `dispatchOTGWInputLine()`.
- `src/OTGW-firmware/OTDirect.ino` — OTDirect producer re-wire in
  `bridgeFrameToParser()`.
- `src/OTGW-firmware/OTGW-firmware.ino` — `setupOTConcurrency()` in `setup()`,
  `drainOTFrameQueue()` in `loop()`.
- `src/OTGW-firmware/restAPI.ino` — reader-side `OTStateLock` in `sendOTmonitorV2()`.
- `src/libraries/Platform/src/platform_esp32.h` — `platformMutex*` / `platformQueue*`
  shims.
- `evaluate.py` — `check_ot_frame_queue_producer_region` (the Phase-1 CI gate).

## Enforcement

Declarative regex (adr-judge) does not fit this rule, which needs Python-level
multi-file confinement and call-site counting. The gate is therefore a code check in
`evaluate.py`, not an `adr-judge` `forbid_pattern` block (ADR-080 is satisfied by a
named CI gate either way).

`evaluate.py::check_ot_frame_queue_producer_region` asserts, on every run:

1. `enqueueOTFrame()` call sites appear **only** in the two producer files
   (`OTGW-Core.ino`, `OTDirect.ino`), and there are at least two of them. A call
   anywhere else means a third producer slipped in.
2. The queue and mutex are created exactly once via `setupOTConcurrency()` (with
   `platformQueueCreate`/`platformMutexCreate`), called exactly once from `setup()`.
3. The `OTStateLock` RAII helper exists in `OTGW-Core.h` and `processOT()` acquires
   it in its body (covering all five `processOT()` call sites, not just the consumer).
4. The OTGWSerial frame-**ingest** read surface (`.available()`/`.read()`) is confined
   to the producer file. This is a partial down-payment: full firmware-wide OTGWSerial
   confinement ("only the PIC task may reference OTGWSerial") lands in seq6, per
   ADR-123 §Enforcement.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
