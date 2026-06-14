# ADR-136 Retire / Simplify the WiFi-Reconnect, Webhook, and PIC PR= State Machines (ADR-123 Phase 4)

## Status

Accepted, 2026-06-14. Proposed 2026-06-14; accepted by the maintainer 2026-06-14.

This is the **Phase 4 cleanup** that ADR-123 (the 2.0.0 concurrency-model
decision) named in its §Rollout: "Retire the now-redundant ESP32-side manual
state machines (ADR-047/048/058 ESP32 variants)." ADR-123 §Consequences-Positive
stated the expected outcome verbatim: "the ESP32-side manual state machines
(ADR-047/048/058) disappear; new I/O-bound features no longer each need a bespoke
non-blocking rewrite." Phases 1-3 (ADR-129/130 PIC task, ADR-131 espMqttClient,
ADR-132/133/134 AsyncWebServer/WebSocket/OTA) put the I/O-bound subsystems on
their own tasks / async callbacks. With those landed, a feature that runs off the
cooperative loop MAY block again, so the hand-rolled non-blocking FSMs built only
to avoid blocking can collapse.

The investigation (TASK-865.13) found the three machines are **not** uniformly
redundant. This ADR records the per-machine verdict — what survives, what
collapses, and why — so the next maintainer does not "finish the job" by deleting
behaviour that was never about blocking-avoidance.

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-4 retirement/simplification of ADR-047/048/058 delivered by TASK-865.13
    changed_via: adr-kit
  - date: 2026-06-14
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted the Phase-4 per-machine verdict (webhook FSM collapsed onto a dedicated task; WiFi-reconnect and PIC PR= machines retained with rationale); supersedes ADR-048 on the 2.0.0 line
    changed_via: manual

## Context

The 2.0.0 firmware inherited three manual non-blocking machines whose sole reason
to exist was the ESP8266-era cooperative single-loop rule: every subsystem had to
return within a few milliseconds, so anything that waited on I/O was chopped into
a state machine or queue interaction (ADR-123 §Context). The three are:

- **ADR-047** — non-blocking WiFi reconnect (`loopWifi()` in `networkStuff.ino`),
  a `WifiState_t` FSM that spreads `WiFi.begin()` + association polling across loop
  turns.
- **ADR-048** — non-blocking webhook (`evalWebhook()` in `webhook.ino`), a
  `WH_IDLE`/`WH_PENDING`/`WH_RETRY_WAIT` FSM that spreads a synchronous
  `HTTPClient` GET/POST and its 3-attempt/30s retry across loop turns.
- **ADR-058** — non-blocking PIC `PR=` (`queryNextPICsetting()` /
  `queryOTGWgatewaymode()` / `getpicfwversion()` in `OTGW-Core.ino`), replacing the
  old synchronous `executeCommand()` busy-wait with a fire-and-forget command queue
  + async `handlePRresponse()`.

Phase 4 asks, per machine: now that the subsystem can run off-loop and block, does
the FSM still earn its keep?

## Decision

Treat the three machines independently. The discriminator is whether the FSM
encodes **real product behaviour** or only **blocking-avoidance mechanics**.

### 1. ADR-058 (PIC `PR=`) — SATISFIED, no code change

ADR-058 is already the correct fire-and-forget pattern, not a blocking-avoidance
FSM. `queryNextPICsetting()`, `queryOTGWgatewaymode()`, and `getpicfwversion()`
each call `addCommandToQueue(cmd, len, /*forceQueue=*/true)` and return
immediately; the response is decoded asynchronously in `handlePRresponse()` from
`processOT()`. There is no busy-wait to remove.

Under the Phase-1 PIC task (ADR-130), that command queue **is the task ingress**:
TX is drained from the queue to `OTGWSerial` by the dedicated PIC task, and the
`PR:` response flows back through the OT-frame queue (ADR-129) to the loop-side
consumer that runs `handlePRresponse()` under `OTStateLock`. The queue pattern
survives unchanged and is exactly what a task-based world wants. ADR-123's
"retire ADR-058" is therefore satisfied **by the queue surviving**, not by
deleting code. `queryNextPICsetting()` and `handlePRresponse()` are byte-for-byte
unchanged by Phase 4.

### 2. ADR-047 (WiFi reconnect) — RETAINED in the loop

`loopWifi()` does **not** block: `WiFi.begin()` is already asynchronous and each
`loopWifi()` call is a sub-millisecond non-blocking poll of `WiFi.status()`. It is
exactly the "residual periodic work that is not I/O-blocked" that ADR-123 §model-3
says **stays in `loop()`**. Moving it to a task would buy zero throughput (the
association wait is already async) while creating real cost:

- `WIFI_RECONNECTED` restarts services (`startTelnet()` / `startMQTT()` /
  `startWebSocket()`); on a separate task these run concurrently with the AsyncTCP
  and espMqttClient tasks for no benefit.
- WiFi↔Ethernet failover is a **loop-coordinated handshake**: both `loopWifi()`
  and `loopEthernet()` read `state.net.eMode` in loop context, and `loopWifi()`
  early-returns when `eMode == NET_ETHERNET`. Splitting WiFi off-loop would break
  that coordination and invite exactly the field-only-untestable race ADR-123
  §Constraints warns is the hardest class to catch.

Beyond blocking-avoidance, `loopWifi()` carries four pieces of **product
behaviour** that MUST survive and do, untouched:

1. **Bounded-retry-then-reboot:** `wifiRetryCount >= 10 → WIFI_FAILED →
   doRestart()` (production builds).
2. **BETA captive-AP fallback:** `WIFI_AP_FALLBACK` / `WIFI_AP_RETRY`, gated on
   `#if defined(_VERSION_PRERELEASE)` — after 2 failed retries the device raises a
   SoftAP and retries WiFi every 5 min instead of rebooting.
3. **NET_ETHERNET preemption early-return:** the wired link owns the radio; the
   FSM stays dormant while Ethernet is active.
4. **Issue #525 / TASK-432 DHCP-ownership rule:** never start DHCP while
   associated; `platformRestartDHCP()` is deliberately absent and must not return.

Verdict: **leave `loopWifi()` logic intact**, record retention here. ADR-047's
mechanism (a non-blocking WiFi FSM) is no longer load-bearing for
blocking-avoidance, but the FSM is retained as the host for the four product
behaviours above. ADR-047 is **amended** by this ADR (retain-in-loop rationale
recorded), not superseded by a code change; it remains Superseded-by ADR-075 for
its timeout parameters.

### 3. ADR-048 (webhook) — SUPERSEDED, collapsed onto a dedicated task

ADR-048 is the cleanest retirement: it wrapped a genuinely **synchronous**
`HTTPClient` GET/POST (the only real blocker of the three) in a loop-spread FSM
purely so the ≤1s round-trip would not stall the cooperative loop. With Phase 4 the
send moves onto a dedicated FreeRTOS task that MAY block, and the loop-spreading
becomes incidental and is deleted.

New shape (`webhook.ino`):

- **Detection stays in the loop.** `evalWebhook()` keeps `evalTriggerBit()` edge
  detection (intrinsic, cheap, pure-read), edge-triggered OFF↔ON semantics, and
  latest-state-supersedes (`webhookLastState` always tracks the newest edge). It
  runs in the same loop context as the OT consumer, so its OpenTherm-state reads
  need **no** `OTStateLock`.
- **A dedicated `webhook` task is the sole sender.** `evalWebhook()` builds a
  self-contained, value-copyable `WebhookJob` (resolved URL, pre-expanded payload,
  content-type, GET/POST flag) and enqueues it on a depth-2 FreeRTOS queue. The
  task blocks on the queue and performs the blocking `HTTPClient` send, running the
  3-attempt / 30s-backoff retry as a real `platformTaskDelay(30000)` instead of a
  loop-spread `WH_RETRY_WAIT`.
- **The sender is lock-free by construction.** The job carries everything the
  send needs, so `sendWebhookJob()` touches no settings, no `OTGWState`, and **never
  holds `OTStateLock` across the blocking HTTP call** — the rule that keeps the
  webhook send from stalling `drainOTFrameQueue` (ADR-129). Payload expansion
  (`expandPayload()`, which reads `OTcurrentSystemState`) happens at **enqueue**
  time: in loop context for the change path (reads already consistent), and under a
  **brief** `OTStateLock` (no I/O held) for the REST test endpoint, which runs on
  the AsyncTCP task.
- **One-pending-at-a-time + convergence preserved (ADR-057 §5).** A
  `webhookInFlight` gate keeps a second job out of the queue until the task
  finishes the current send+retry cycle. `webhookLastState` is the last state the
  loop **acted on** (enqueued), and is latched **only** when a job is enqueued —
  never while a send is in flight. So a toggle observed during a long retry does
  not advance the latch; once the gate clears, the next loop tick re-detects the
  **live** bit and enqueues it, converging on the most recently observed state
  without building a backlog. This mirrors the old FSM, which assigned
  `webhookLastState` only on the `WH_IDLE → WH_PENDING` transition and never
  during `WH_PENDING`/`WH_RETRY_WAIT`.
- **REST test endpoint stops blocking the AsyncTCP task.** `testWebhook()` now
  enqueues a job and returns immediately rather than calling the synchronous send
  inline (which, post-ADR-132, would block the AsyncTCP service task).

Verdict: **ADR-048 is superseded.** Its `WH_*` loop-spread machine is deleted; the
two behaviours that were ever real — `evalTriggerBit()` change detection and the
3-attempt/30s retry — survive, the first in the loop and the second in the task.

### ADR-057 re-validation (webhook delivery contract)

ADR-057 (the policy-level webhook contract) is re-validated against the new code
and holds:

- §3 local-only outbound: `isLocalUrl()` is enforced **before every send**, inside
  `sendWebhookJob()` on the task, on each of the up-to-3 attempts. A policy block is
  non-retryable and reported as success (no retry storm). Unchanged.
- §4 delivery/timeout: one HTTP request per attempt; HTTP 2xx is success. The
  per-attempt timeout is the ADR-057 §4 contract value (1000 ms). The cooperative-era
  500 ms loop-stall cap is no longer needed because the send is off-loop — the task
  may block.
- §5 retry: initial attempt plus up to 3 total, 30 s between; one pending transition
  retained at a time; best-effort, not guaranteed. Unchanged in contract,
  reimplemented as a task-side loop.
- §6 protected test endpoint: route, validation, and the ADR-056 admin/CSRF boundary
  are untouched in `restAPI.ino`; only the send mechanism behind `testWebhook()`
  changed (enqueue instead of inline blocking send).
- The ADR-004 error path (`snprintf_P("HTTP error %d")`, never
  `http.errorToString()` → `String`) is preserved verbatim on the task.

## Alternatives Considered

### Alternative 1 — Move WiFi onto its own task too (uniform "everything is a task")
Lift `loopWifi()` into a FreeRTOS task alongside the PIC and webhook tasks for
symmetry.

**Pros:** one mental model (every subsystem is a task).
**Cons:** zero benefit (WiFi association is already async; the poll is sub-ms);
concurrent service restarts for no throughput gain; breaks the loop-coordinated
Ethernet-failover handshake; adds a hard-to-field-test concurrency surface.
**Rejected:** ADR-123 §model-3 explicitly keeps non-I/O-blocked periodic work in
`loop()`; WiFi monitoring is exactly that. Symmetry is not a reason to spend the
race budget ADR-123 §Risks flags as scarce.

### Alternative 2 — Keep the webhook FSM, just relax its timeout
Leave the `WH_*` loop machine and raise the timeout now that headroom exists.

**Pros:** smallest diff.
**Cons:** keeps the loop coupled to a blocking HTTP send (the AsyncTCP/loop stall
Phase 4 exists to remove), and keeps the test endpoint blocking the AsyncTCP task.
**Rejected:** the webhook send is the one genuine blocker of the three; not moving
it off-loop forfeits the whole point of Phase 4 for this subsystem.

### Alternative 3 — Expand the webhook payload on the task under OTStateLock
Let the sender task hold `OTStateLock` while it expands `{vars}` and sends.

**Pros:** detection enqueues only a bool; tiny queue item.
**Cons:** the lock would be held across the blocking HTTP round-trip (≤1s),
stalling `drainOTFrameQueue` — re-creating the exact stall Phase 4 removes.
**Rejected:** the load-bearing rule is "never hold the state mutex across blocking
I/O." Expanding at enqueue time and shipping a self-contained job keeps the sender
lock-free.

### Alternative 4 — Delete ADR-058's `PR=` queue and call the PIC task directly
Treat ADR-058 as redundant and bypass `addCommandToQueue()`.

**Pros:** one fewer indirection.
**Cons:** the command queue carries retry/dedup (ADR-016) and is the PIC task's TX
ingress under ADR-130; bypassing it would re-hand-roll both.
**Rejected:** the queue is the right shape for the task world; ADR-058 is satisfied
by keeping it.

## Consequences

### Positive
- The webhook blocking send leaves the cooperative loop and the AsyncTCP task; new
  webhook-class features no longer need a bespoke non-blocking rewrite (the ADR-123
  Phase-4 goal).
- The webhook send can no longer stall `drainOTFrameQueue`, the OT parser, MQTT, or
  the web UI — it is a lock-free task fed by loop-side expansion.
- WiFi product behaviour (retry-cap reboot, BETA AP fallback, Ethernet preemption,
  issue-#525 DHCP rule) is preserved exactly, with no new concurrency surface.
- The PIC `PR=` path is unchanged; the proven queue pattern carries straight into
  the task world.

### Negative
- A third application FreeRTOS task (`webhook`) joins `picSerial` and the AsyncTCP /
  BLE tasks. Its ~4 KB stack is negligible on the ESP32-S3 but is one more task to
  reason about. Mitigation: it is a pure sender that blocks on its queue (zero CPU
  when idle) and touches only its job — the smallest possible task contract.
- `webhookInFlight` is a cross-context flag (set in the loop, cleared on the task).
  It is a single `volatile bool` used as a coarse gate, not for data hand-off (the
  data crosses via the value-copy queue), so it needs no mutex; a benign race only
  ever drops or delays one best-effort webhook, which the contract already permits.

### Risks & Mitigation
- **Risk:** the webhook task blocks 30 s during backoff and trips a watchdog.
  **Mitigation:** like the PIC task, the webhook task is **not** subscribed to the
  ESP32 TWDT (ADR-135 subscribes the loop task only), so a long `platformTaskDelay`
  is safe by design.
- **Risk:** a future maintainer "finishes Phase 4" by also deleting `loopWifi()`.
  **Mitigation:** this ADR records the four WiFi invariants and the explicit
  retain-in-loop verdict; the four-invariant evaluator/grep checks in TASK-865.13
  guard against silent regression.

## Related Decisions

- **Phase parent:** ADR-123 (2.0.0 concurrency model; §Rollout Phase 4 is this ADR).
- **Builds on:** ADR-129 (OT-frame queue + `OTGWState` mutex), ADR-130 (PIC task;
  the `PR=` queue is its TX ingress), ADR-131/132/133/134 (async MQTT/web/WS/OTA —
  the off-loop world that lets the webhook send block again).
- **Supersedes:** ADR-048 (non-blocking webhook FSM — collapsed onto the webhook
  task).
- **Amends:** ADR-047 (non-blocking WiFi reconnect — retained in the loop as the
  host for product behaviour; its timeout params remain Superseded-by ADR-075).
- **Satisfies without change:** ADR-058 (non-blocking PIC `PR=` — the queue pattern
  is the PIC-task ingress and survives unchanged).
- **Re-validated:** ADR-057 (webhook delivery/retry/test-endpoint contract — holds
  against the new code). Preserves ADR-003/032 (local-only outbound), ADR-004 (no
  String error path), ADR-056 (test endpoint admin/CSRF boundary), ADR-044
  (single-point task creation).

## References

- `src/OTGW-firmware/webhook.ino` — `evalWebhook()` (detect+enqueue),
  `buildWebhookJob()`, `sendWebhookJob()`, `webhookTaskBody()`, `startWebhookTask()`,
  `testWebhook()`.
- `src/OTGW-firmware/networkStuff.ino` — `loopWifi()` / `WifiState_t` (retained).
- `src/OTGW-firmware/OTGW-Core.ino` — `queryNextPICsetting()` /
  `handlePRresponse()` (unchanged).
- `src/OTGW-firmware/OTGW-firmware.ino` — `startWebhookTask()` call in `setup()`.
- `src/libraries/Platform/src/platform_esp32.h` — `platformQueueCreate/Send/Receive`,
  `platformTaskCreatePinned`, `platformTaskDelay` shims.
- ADR-123, ADR-047, ADR-048, ADR-057, ADR-058, ADR-075, ADR-129, ADR-130, ADR-135.
