# ADR-123 Phase-1 Appendix: Single-Writer-Per-Field Map (TASK-865.5)

Date: 2026-06-13
Scope: ADR-123 (2.0.0 concurrency model) Rollout Phase 1 foundation.
Status: Phase-1 foundation delivered in isolation (no behaviour change). The
producer is NOT yet lifted into a FreeRTOS task; that is seq6 (TASK-865.6).

## Purpose

ADR-123 Rollout Phase 1 introduces two concurrency primitives so that, when seq6
moves the PIC-UART producer onto a FreeRTOS task, the cross-task data flow is
already disciplined:

1. A FreeRTOS **value-copy queue** carrying parsed OT lines from the producer
   (PIC serial / OTDirect) to a single consumer that calls `processOT()` in
   `loop()` context. The queue copies by value, so the **frame flow needs no
   mutex** and FIFO order is preserved (byte-identical OT-log/MQTT output).
2. A single FreeRTOS **mutex** (`otStateMutex`) + RAII `OTStateLock` guarding the
   decoded **snapshot** that is read cross-task: `OTcurrentSystemState.*`,
   `state.otBus.*`, and `state.sat.*`.

The frame queue and the snapshot mutex are two *different* problems: the queue is
about getting raw lines across the task boundary in order; the mutex is about a
reader on one task seeing a torn multi-byte field while a writer on another task
updates it. This appendix records, per shared field group, **who writes it and
in which context**, so the single-writer-per-field rule can be enforced and the
genuine exceptions are documented rather than hidden.

## Method

`grep` for every assignment (`=`, `++`, `--`, `+=`, `-=`) of
`OTcurrentSystemState.*`, `state.otBus.*`, and `state.sat.*` across
`src/OTGW-firmware/*.ino` and `*.cpp` (the auto-generated `*.ino.cpp` is excluded
— it is a concatenation of the `.ino` sources, not an independent writer). Reader
contexts were located by `grep` for any reference in the consumer files.

## The map

| Field group | Owner-writer context | 2nd write site? | Reader contexts | Lock discipline (Phase 1) |
|---|---|---|---|---|
| `OTcurrentSystemState.*` decoded OT values (TSet, Tboiler, Tret, Statusflags, RelModLevel, counters, …) | `processOT()` (OTGW-Core.ino) — invoked by the queue consumer `drainOTFrameQueue()` AND by the four OTDirect synth sites | **No**, except `Toutside` (see below) | `restAPI.ino` (`sendOTmonitorV2`, `satSendHealthJSON`), `MQTTstuff.ino`, `OLED.ino`, `SATcontrol/cycles/pressure/weather.ino`, `outputs_ext.ino`, `webhook.ino` | Writer takes `OTStateLock` **inside `processOT()`** — covering all five call sites uniformly, not just the consumer; reader `sendOTmonitorV2()` takes `OTStateLock`. |
| `OTcurrentSystemState.Toutside` | `processOT()` decode of MsgID 27 (OTGW-Core.ino) | **Yes** — `handleOTDirectCommand()` `OT=` command handler (OTDirect.ino:2753) writes it directly when the user injects an outside temperature | same as above | Both writers run in `loop()` context in Phase 1 (no race). When seq6 lifts the PIC producer onto a task, the `OT=` handler still runs on the loop task, so `Toutside` becomes a **two-task** write target — seq6 must either route the `OT=` write through the producer or take `OTStateLock` around it. Flagged here as the one OT-value field that is NOT single-writer. |
| `state.otBus.bOnline`, `bBoilerState`, `bThermostatState` | `processOT()` (OTGW-Core.ino) on the PIC path | **Yes** — OTDirect.ino sets `state.otBus.bOnline` on its own request/response/timeout path (lines 860/866/1207/1261/1371) | `restAPI.ino`, `MQTTstuff.ino`, `OLED.ino` (liveness) | **Single-writer-per-boot-mode** (ADR-127): a combo board boots EITHER PIC mode OR OTDirect mode; only one of the two writer paths is compiled-live/active at runtime, so there is exactly one writer per boot. Documented exception: not a clean single writer in source, but mutually exclusive at runtime. |
| `state.otBus.bPSmode` | `processOT()`/`enterPSMode`/`leavePSMode` (OTGW-Core.ino) | No | `restAPI.ino`, OT-log | Writer side under `OTStateLock` via the consumer; PS-mode toggles ride the same processOT call chain. |
| `state.otBus.bGatewayMode`, `bGatewayModeKnown` | `processOT()` PR-response handler (OTGW-Core.ino:703-704) | No | `restAPI.ino` | Same processOT writer context. |
| `state.sat.*` controller fields (setpoints, PID terms, cycle state, mode) | The SAT control loop: `SATcontrol.ino`, `SATcycles.ino`, `SATpid.ino`, `SATpressure.ino`, `settingStuff.ino` (config apply) | No (single owner = SAT loop) | `restAPI.ino` (SAT health/serialize), `MQTTstuff.ino` (SAT publish) | All SAT-loop writers run in `loop()` context (`satControlLoop()`); no cross-task writer. Phase-1 reader lock not yet required, but the field group is named so seq6+ networking tasks that read SAT state acquire `OTStateLock`. |
| `state.sat.*` BLE-sourced fields (`fBleTemp`, `bBleTempValid`, `iBleBattery`, `iBleRssi`, `iBleTempLastMs`, `iBleSensorCount`) | `satBLEUpdateState()` (SATble.ino) — runs in `loop()` context | No | `restAPI.ino`, `MQTTstuff.ino` | The BLE **scan callback** runs on the NimBLE FreeRTOS host task (core 0) and writes `_bleSensors[]` (the raw slot array), guarded by the existing `_bleSensorsMux` portMUX spinlock (SATble.ino:127). The loop task takes a snapshot of `_bleSensors[]` under that spinlock, then writes `state.sat.*` from the snapshot — so `state.sat.*` itself has a single (loop) writer. `_bleSensorsMux` is intentionally NOT reused for `otStateMutex` (different object, different scope). |

## Honest exceptions (fields that are NOT clean single-writer)

- **`OTcurrentSystemState.Toutside`** — two writers (bus-decode + `OT=` command).
  Both loop-context today; a true second-task hazard once seq6 lands. **Action
  for seq6:** route the `OT=` write through the producer or wrap it in
  `OTStateLock`.
- **`state.otBus.bOnline`** (and the two state booleans) — two writers
  (PIC `processOT` vs OTDirect), but **mutually exclusive per boot mode**
  (ADR-127). Single-writer *per boot*, not single-writer in source.

Every other listed field has exactly one write context.

## Producer/consumer seam (the four direct-processOT exceptions)

The two **frame-bus producers** now enqueue instead of calling `processOT()`
inline:

- PIC: `handlePICSerial()` → `dispatchOTGWInputLine()` → `enqueueOTFrame(line, len, false)`
- OTDirect: `bridgeFrameToParser()` → `enqueueOTFrame(buf, 9, otHideReports)`

Four OTDirect sites keep calling `processOT()` **directly, by design** — they are
command-RESPONSE synthesis, not frame-bus producers, and their state updates must
stay synchronous with the command path (see the `synthesizeResponse` comment at
OTDirect.ino):

- `otDirectBridgeProcessStatus()` (2-char status injection)
- the OTDirect statistics-line builder (PS=1-style summary)
- `synthesizeResponse()` (`XX: value` command echo)
- `otDirectBridgeProcessPRResponse()` (`PR:` response)

Routing these through the queue would defer their state updates to loop-end and
change command-path behaviour, so they are excluded from the producer seam. The
`evaluate.py` gate `check_ot_frame_queue_producer_region` therefore confines the
**`enqueueOTFrame` call symbol** (not `processOT`, and not `OTGWSerial`, which is
used firmware-wide for reset/flash/command-send/banner-detect).

## Why `OTFrameMsg.line` is `MAX_BUFFER_READ` (512), not 10

The TASK-865.5 prose suggested `char line[10]`. Primary-source evidence
overrides it: `processOT()` is fed the **full** PIC line, not only 9-char OT
frames. Banners (`OpenTherm Gateway…`), `=`-command echoes (`PS=0`, `TT=20.0`),
and PS=1 comma-separated summaries (which can exceed 256 bytes — the reason
`MAX_BUFFER_READ` is 512) all flow through `processOT()`'s `else if` branches and
produce observable MQTT/WebSocket/OT-log output. A 10-byte item would truncate
them and break byte-identical replay. The struct therefore carries
`char line[MAX_BUFFER_READ]`, `uint16_t len` (511 > 255), and the producer
null-terminates `line[len]` because the non-OT branches use
`strstr`/`strchr`/`strcasecmp_P`, which read until a terminator.

## Verification (build/evaluator-verifiable ACs)

- `OTFrameMsg` is fixed POD with `static_assert(std::is_trivially_copyable<...>)`.
- Mutex + queue each created exactly once in `setupOTConcurrency()` (called once
  from `setup()`), per ADR-044.
- Consumer (`drainOTFrameQueue`) calls `processOT()` from `loop()` (after
  `doBackgroundTasks()`), NOT from a task and NOT inside `doBackgroundTasks()`.
- `processOT()` acquires `OTStateLock` in its body, so the four OTDirect
  command-synthesis sites that call `processOT()` directly are ALSO protected —
  the consumer-site wrap was removed to avoid a non-recursive self-deadlock.
- `evaluate.py::check_ot_frame_queue_producer_region` asserts: enqueue
  confinement + create-once + `OTStateLock` present + processOT acquires it +
  OTGWSerial ingest surface confined to the producer file.

## Not verifiable here (field-validation ACs)

- **Byte-identical OT-log/MQTT replay**: satisfied *by construction* (FIFO
  drain-to-empty each loop iteration, no drops under the per-poll burst,
  `suppressOutput` carried in the item, no setup-time enqueue path that would
  defer the first frames). Empirical replay needs the `/otgw_simulation.log`
  device path and belongs to the ESP32-S3 soak AC. Reasoned, not empirically run.
- **Race-freedom under concurrent PIC-task + AsyncTCP load + combo dual-boot
  routing**: there is no automated concurrency test; this is the #dev-sat-mqtt
  soak AC and is out of scope for this (no-hardware) task.

## Note for seq6 (lock-scope tightening)

`sendOTmonitorV2()` currently holds `OTStateLock` across the entire JSON stream.
That is fine in Phase 1 (cooperative loop, uncontended). When seq6 makes the
producer a real task, the reader holding the lock for a whole HTTP stream would
stall the PIC task; seq6 must shorten the critical section to a
snapshot-under-lock, then stream the snapshot outside the lock (the pattern the
BLE path already uses with `_bleSensorsMux`).
