# ADR-138 PIC-UART Control-Method Park Handshake, TX Requeue-to-Front Ordering, and Progress-Path Heap-Gate Decision (Amends ADR-130 and ADR-133)

## Status

Proposed, 2026-06-14.

This ADR amends two Accepted ADRs. It tightens **ADR-130** (PIC-UART dedicated
FreeRTOS task as sole OTGWSerial owner) with two invariants its sole-owner CI gate
structurally cannot catch, and it adds a **note to ADR-133** (WebSocket live-log on
AsyncWebSocket) recording a conscious heap-gate decision for the firmware-upgrade
progress path. Both ADR-130 and ADR-133 stay in force and Accepted; this ADR
generalizes/annotates them rather than reversing anything. Per the immutability
convention, the amendment lives in this new document rather than editing either
Accepted ADR body. The "Amended by" back-references on ADR-130 and ADR-133, if any,
are the maintainer's to add at the acceptance checkpoint; this ADR expresses the
relationships from its own side (title and Related Decisions). The same pattern this
project already uses for ADR-137 (amends ADR-132), ADR-135 (amends ADR-011), and
ADR-089 (amends ADR-030).

Delivered by TASK-865.15. Field validation on ESP32-S3 hardware is open (see Risks)
and tracked under epic TASK-865.

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: "ADR-130 follow-up (recon 2026-06-14). Close residual PIC-UART task-coupling gaps the sole-owner gate does not catch: (A) control-method UART access (resetPic()'s direct GW=R write and detectPIC()'s resetPic()+find(ETX) probe) must run under the waitForPICTaskParked() handshake so the loop is never a second concurrent UART owner; (B) the TX short-write requeue must go to the FRONT of otTxQueue (xQueueSendToFront) so a concurrently-enqueued ser2net relay byte cannot jump ahead of a popped command chunk and interleave its bytes. Also adds the sim-mode-entry park and the fwupgradestep integer-pct WebSocket dedupe, and records the ADR-133 progress-path heap-gate decision. Delivered by TASK-865.15; amends ADR-130 and ADR-133."
    changed_via: adr-kit

## Context

ADR-130 made the dedicated FreeRTOS PIC task the **sole UART byte-I/O owner**: all
PIC reads/writes are confined to three task-owner functions, and `evaluate.py`'s
`check_pic_uart_task_owns_serial` gate forbids new byte-I/O (`.read`/`.write`/
`.available`/`.availableForWrite`/`.flush`/`.peek`) outside them. A 2026-06-14 recon
found gaps that gate cannot catch by construction, because each one sits at a site the
gate treats as out of scope:

### Gap A: control-method UART access runs unparked

The sole-owner gate matches only **byte-I/O** methods; **control** methods
(`begin`/`end`/`resetPic`/`startUpgrade`/…) are explicitly allowlisted. That allowlist
is correct for control methods that do not drive the UART data line, but two of them
do, via the vendored `OTGWSerial` (out of scope, not modifiable):

- `OTGWSerial.resetPic()` does a **direct UART write** of `"GW=R\r"` followed by a
  reset-GPIO toggle and a `delay(100)`. It bypasses `otTxQueue` and is gated by no
  park condition. `resetOTGW()` → `resetPic()` is called **loop-side** from two
  runtime paths while the PIC task is **unparked** (a reset flips none of the existing
  park conditions): the ser2net `GW=R` path (`handlePICSerial`) and the MQTT
  `resetgateway` command (`MQTTstuff.ino`).
- `detectPIC()` calls `OTGWSerial.resetPic()` (direct write) **and then**
  `OTGWSerial.find(ETX)` (an inherited `Stream` read loop, i.e. concurrent UART
  byte-I/O). At boot both run **before** `startPICSerialTask()`, so there is no task to
  race; but the manual `'p'` telnet debug command (`handleDebug.ino`, gated only on
  `!isOTDirectEnabled()`) reaches `detectPIC()` at runtime on a `HAS_PIC` build while
  the task is unparked.

In each window the loop is a **second concurrent UART owner** alongside the task drain,
the exact failure mode ADR-130's Alternative D was rejected to prevent. The payloads are
tiny and idempotent (a reset, a single ETX probe), so severity is low, but the invariant
must hold without exception. The two sibling `resetPic`-adjacent paths are NOT racy and
need no change: the automatic 60 s PIC re-probe enqueues via `enqueuePICTx` (not
`detectPIC`), and the `'a'` debug `PR=A` goes through `addCommandToQueue`.

### Gap B: TX short-write requeue can reorder command bytes

ADR-130 §TX said a short write "re-queues the chunk for the next tick to preserve order",
and the code re-queued to the **back** of `otTxQueue`, reasoning that "only the task pops".
That reasoning was incomplete. The ser2net relay (`handlePICSerial`) enqueues **one byte
per `OTTxMsg` from the loop side**, concurrently with the task. So between the task popping
a chunk, finding the UART TX buffer full, and re-queuing that chunk to the back, a relay
byte enqueued loop-side can land **ahead** of the popped chunk. On the next drain pass the
relay byte is written first, interleaving a command's bytes: silent command corruption,
worse than the silent-drop trade-off ADR-130 documented and accepted.

### Two adjacent task-coupling races (rolled into this task)

- **Sim-mode entry.** `handlePICSerialSimulation()` calls `picSerialFlushRx()` loop-side
  on the sim-mode entry tick with no `waitForPICTaskParked()` handshake. On that tick
  `state.debug.bOTGWSimulation` has only just flipped true, so the task may still be
  mid-drain: a one-tick double-read race between the loop flush and the task drain on
  `OTGWSerial.read()`.
- **Progress churn / heap pressure (ADR-133 surface).** `fwupgradestep()` throttled only
  the Debug log; `sendWebSocketJSON()` → `otLogWs.textAll()` fired on **every** progress
  callback (many at the same pct), each a per-client heap alloc on the AsyncTCP task during
  the most heap-sensitive operation of the firmware (a PIC flash). It also bypasses the
  ADR-121 heap gate that `sendLogToWebSocket()` honors. That asymmetry was incidental, not
  decided.

## Decision

### A. Control-method UART access must run under the park handshake

ADR-130's sole-ownership contract extends to **control methods whose body drives the UART
data line**. Concretely: a new unconditional file-scope flag `g_picResetInProgress` is
OR-ed into `picSerialTaskShouldPark()`. Any loop-side site that calls a UART-driving
`OTGWSerial` control method outside the three task-owner functions raises the flag, calls
`waitForPICTaskParked()`, performs the direct access, then clears the flag:

- `resetOTGW()` wraps `OTGWSerial.resetPic()`.
- `detectPIC()` wraps the whole `OTGWSerial.resetPic()` **plus** `OTGWSerial.find(ETX)`
  span (both the direct write and the ETX read loop are inside the park).

The flag is declared **outside** `#if HAS_PIC` because `picSerialTaskShouldPark()` (which
reads it) is compiled on every build (incl. `HAS_PIC==0`, where `resetOTGW()`/`detectPIC()`
are no-ops). The vendored `resetPic()`/`find()` are left untouched (out of scope); the park
is applied entirely on the firmware side. At boot both `resetOTGW()` and `detectPIC()` run
**before** `startPICSerialTask()`, so `waitForPICTaskParked()` returns immediately (no task
yet) and adds no startup latency; the park only engages for the runtime paths (ser2net `GW=R`,
MQTT `resetgateway`, manual `'p'` debug command).

**Invariant (amended over ADR-130):** any code path that drives the PIC UART **outside** the
three task-owner functions (including a control method whose body writes or reads the UART,
of which `resetPic()` and `detectPIC()`'s `find(ETX)` are the only instances) must first park
the task via the `waitForPICTaskParked()` handshake and hold the park for the duration of the
direct access. As of TASK-865.15 there are no remaining unparked direct-UART loop paths.

### B. TX short-write requeue goes to the FRONT (ordering invariant)

On a flow-control short write, the popped chunk is re-queued to the **front** of `otTxQueue`
via `platformQueueSendToFront()` (a new ADR-120 shim over FreeRTOS `xQueueSendToFront`), so it
remains the next thing written and no concurrently-enqueued chunk (e.g. a loop-side ser2net
relay byte) can jump ahead of it. Only one chunk is ever re-queued per drain pass (the task
`break`s immediately after), so a single send-to-front fully restores strict FIFO. Head-of-line
blocking when a chunk does not fit is the correct ordering to protect, not a regression; chunks
are command-/1-byte-sized against a 128-byte-plus UART buffer, so a wedge is near-impossible.

This supersedes the "re-queue at the back … preserves order" wording in ADR-130's original
Decision §TX; nothing else in ADR-130's Decision changes, and the byte-I/O sole-owner gate is
untouched and still passes.

### C. Sim-mode entry parks before the loop-side flush

`handlePICSerialSimulation()` calls `waitForPICTaskParked()` before `picSerialFlushRx()` on the
sim-mode entry tick. `state.debug.bOTGWSimulation` is already a `picSerialTaskShouldPark()`
condition, so once parked the handshake returns on the first poll at zero cost on later ticks.

### D. Firmware-upgrade progress WebSocket is deduped on integer-pct, deliberately ungated

`fwupgradestep()` **dedupes the WebSocket progress broadcast on integer-percent change**: the
`OTGWSerial` callback fires many times per percent, but only a distinct pct triggers a broadcast
(at most ~101 sends spread over the whole flash, ~2/s, not the unbounded per-callback stream).
A function-local `static int lastSentPct` carries across flashes; `pct == 0` (the per-flash
"start" frame) always passes so a retried flash whose predecessor also ended on 0 still emits its
"start". Every `pct > 0` frame is deduped on integer-pct change.

**The firmware-upgrade progress path is intentionally NOT routed through the ADR-121 heap gate**
(`canSendWebSocket()`, which `sendLogToWebSocket()` keeps). This is the deliberate trade-off the
project rule requires to be recorded, not made silently (it is the note this ADR adds to ADR-133):

1. **It is no longer a firehose.** Post-dedupe it is ~101 frames over the whole flash, not an
   unbounded per-callback stream.
2. **Progress is operation state, not a droppable log line.** The heap gate exists to throttle/
   drop the high-rate OT live-log so MQTT does not go unavailable under pressure (ADR-121). A
   flash-progress frame is user-visible operation feedback; silently dropping intermediates under
   heap pressure would freeze the progress bar (e.g. stuck at 99%) for the duration of the flash.
3. **Completion is reliable regardless.** The terminal `"end"`/`"error"` frame is sent
   **independently** by `fwupgradedone()`, not by the progress callback, so a dropped intermediate
   progress frame never leaves the UI without a final state.

The OT-hot-path gate in `sendLogToWebSocket()` is untouched; only the progress path is exempt, and
only after the dedupe makes the exemption cheap.

## Alternatives Considered

### A-alt: Route `resetPic`'s `GW=R` through `enqueuePICTx` instead of parking (rejected)

The task offered "route GW=R through enqueuePICTx OR wrap resetOTGW() in the park handshake." Routing
through the queue was rejected: `resetPic()` is more than a `GW=R` write. Its vendored body also
toggles the reset GPIO and `delay(100)`s, and we may not modify the vendored library to split those
out. Queuing only the `GW=R` text would still leave the GPIO toggle running loop-side against an
unparked task, and `detectPIC()`'s `find(ETX)` read has no queue analog at all. The park handshake
covers the whole control-method span uniformly with the primitive ADR-130 already ships
(`waitForPICTaskParked()`), so it is the smaller, more complete change.

### B-alt: Coalesce the ser2net relay into multi-byte `OTTxMsg` (rejected for this task)

The task offered "requeue-to-front OR coalesce the ser2net relay." Coalescing the per-byte relay into
multi-byte messages would also remove the interleave window, but it is a larger change to the relay's
RX→TX path with its own buffering/latency questions, for no benefit over the one-line requeue-to-front
that fully restores FIFO. Requeue-to-front is the minimal-change-surface fix; coalescing can be
revisited if the relay path is reworked for other reasons.

### C-alt: Gate the progress path on `canSendWebSocket()` for strict ADR-121 consistency (rejected)

Routing `fwupgradestep()` through the ADR-121 heap gate would make the WebSocket TX policy uniform, but
it would require the terminal `fwupgradedone()` frame to **bypass** the gate so completion is never lost
under heap pressure, re-introducing exactly the asymmetry it was meant to remove, plus the risk of a
frozen progress bar. Dedupe + no-gate is the simpler, KISS choice and is recorded here as the conscious
trade-off.

## Consequences

**Benefits**

- No remaining unparked direct-UART loop path: `resetOTGW()` (ser2net `GW=R`, MQTT `resetgateway`) and
  `detectPIC()` (manual `'p'` debug) all run with the PIC task parked, so the loop is never a second
  concurrent UART owner. ADR-130's single-ownership invariant now holds without exception.
- Command bytes can no longer interleave under TX backpressure: the requeue-to-front keeps a popped
  chunk strictly ahead of any concurrently-enqueued relay byte.
- Sim-mode entry no longer double-reads the UART for one tick.
- The PIC-flash live progress no longer churns a per-client heap alloc on the AsyncTCP task on every
  callback; it sends at most ~101 deduped frames over the whole flash.

**Trade-offs**

- `resetOTGW()`/`detectPIC()` now spin up to ~200 ms (the `waitForPICTaskParked()` bound) waiting for
  the task to park on the runtime paths. These are rare, manual/relay-triggered operations, so the
  added latency is immaterial; the spin `feedWatchDog()`s and is bounded so a wedged task cannot hang.
- Head-of-line blocking is now explicit: a chunk that does not fit the UART TX buffer blocks later
  chunks until it drains. This is the correct ordering guarantee, not a regression, and is near-impossible
  to wedge given 1-byte/command-sized chunks against a 128-byte-plus buffer.
- The progress path stays outside the ADR-121 heap gate (decision D), accepted because the dedupe removed
  the firehose and completion is delivered independently.

**Risks and mitigations**

- *Risk*: a future control method (or a future caller of `detectPIC`) drives the UART loop-side without
  raising `g_picResetInProgress`. *Mitigation*: the Enforcement block flags it for the LLM-judge/reviewer;
  the invariant is stated here and cross-referenced from `resetOTGW()`/`detectPIC()`/`picSerialTaskShouldPark()`.
- *Risk*: field behaviour on ESP32-S3 is unverified (AC#7). *Mitigation*: open field-validation AC under
  epic TASK-865: a ser2net load soak must show no command corruption, and a PIC flash with live progress
  must show no heap-churn regression.

## Related Decisions

- **ADR-130 (PIC-UART dedicated FreeRTOS task as sole OTGWSerial owner)**: **amended by this ADR.**
  ADR-130 established the sole-owner boundary and the byte-I/O CI gate; this ADR extends the boundary to
  cover UART-driving control methods (the park handshake) and replaces the "requeue at the back" TX-ordering
  wording with requeue-to-front. ADR-130's body, Status, and byte-I/O Enforcement gate are left untouched
  per the immutability rule, and that gate still passes.
- **ADR-133 (WebSocket live-log on AsyncWebSocket, port 80)**: **amended by this ADR** with the
  progress-path heap-gate note (decision D). ADR-133 grouped both the OT-hot-path live-log and the
  firmware-upgrade progress under one `otLogWs.textAll()` API; this ADR records why the progress path is
  deliberately exempt from the ADR-121 gate that the live-log keeps. ADR-133's body/Status are untouched.
- **ADR-121 (per-consumer heap gating, WebSocket vs MQTT)**: the gate the progress path is consciously
  exempted from (decision D). The OT-hot-path live-log keeps the gate; only the deduped progress path is
  exempt.
- **ADR-120 (platform abstraction promoted to a library)**: the new `platformQueueSendToFront()` shim
  (over `xQueueSendToFront`) lives in `platform_esp32.h` and is called unguarded by application code, per
  the abstraction rule (shim first, then call).
- **ADR-129 (OT-frame queue + OTGWState mutex foundation)**: `otTxQueue` and `waitForPICTaskParked()` are
  the primitives this ADR reuses; no new concurrency primitive is introduced.
- **ADR-004 (no String in hot paths)**: the new code uses `volatile bool` flags, `static int`, and the
  existing `char[]` JSON buffer; no `String` is added.

## References

- Task: TASK-865.15 (harden the PIC-UART FreeRTOS coupling; amend ADR-130 and ADR-133).
- `src/libraries/Platform/src/platform_esp32.h`: `platformQueueSendToFront()` (over `xQueueSendToFront`)
  for the TX requeue-to-front ordering invariant.
- `src/OTGW-firmware/OTGW-Core.ino`:
  - `g_picResetInProgress` (unconditional file-scope park flag) OR-ed into `picSerialTaskShouldPark()`.
  - `resetOTGW()`: park-handshake wrap around `OTGWSerial.resetPic()`.
  - `detectPIC()`: park-handshake wrap around the `OTGWSerial.resetPic()` + `OTGWSerial.find(ETX)` span.
  - `picSerialDrainOnce()`: TX requeue changed from `platformQueueSend` (back) to
    `platformQueueSendToFront` (front).
  - `handlePICSerialSimulation()`: `waitForPICTaskParked()` added before `picSerialFlushRx()`.
  - `fwupgradestep()`: integer-pct WebSocket dedupe (`lastSentPct`), with the ADR-133 heap-gate trade-off
    recorded in-comment.
- `src/OTGW-firmware/handleDebug.ino`: the manual `'p'` command path that reaches `detectPIC()` at runtime.
- ADR-130 (the boundary amended), ADR-133 (the progress note), ADR-121 (the gate exempted), ADR-120 (the
  shim home), ADR-129 (the reused primitives).

## Enforcement

ADR-130's byte-I/O sole-owner gate (`check_pic_uart_task_owns_serial` in `evaluate.py`) is unchanged and
still passes after this amendment: none of these changes add byte-I/O outside the task-owner functions.
The two invariants this ADR adds, however, are **not** expressible as a single forbidden line-symbol:

- The control-method park invariant (Decision A) is a **call-graph / reachability** property: "a loop-side
  call to a UART-driving `OTGWSerial` control method must be bracketed by `g_picResetInProgress` +
  `waitForPICTaskParked()`." The byte-I/O regex does not match `resetPic()`/`find()`, and a blunt
  `forbid_pattern` on those symbols would false-positive on the very `resetOTGW()`/`detectPIC()` sites that
  implement the fix.
- The TX requeue-to-front invariant (Decision B) is a semantic ordering property: a short-write requeue must
  use `platformQueueSendToFront`, not `platformQueueSend`. Both calls are legitimate elsewhere, so a symbol
  ban would over-match.
- The progress-path heap-gate decision (Decision D) is a recorded design choice, not a checkable line.

So the honest fit is an **LLM-judge** advisory (the same shape ADR-137 chose): a reviewer (or the adr-judge
LLM pass) checks that any new loop-side UART-driving control-method call is parked, any new TX requeue
preserves front-ordering, and the progress path's gate exemption stays as decided. The maintainer ratifies
or downgrades this block at the Proposed checkpoint.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": true
}
```
