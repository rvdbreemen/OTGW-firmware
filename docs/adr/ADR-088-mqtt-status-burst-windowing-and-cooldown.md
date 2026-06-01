# ADR-088: MQTT Status Burst Windowing and Post-Burst Cooldown

## Status

Accepted, 2026-04-26.

## Context

A single OpenTherm Status frame (msgid 0) fans out into many MQTT publishes
in rapid succession. The master byte expands into `status_master` plus 7 per-bit
topics; the slave byte expands into `status_slave` plus 8 per-bit topics. In
practice this produces up to 17 publishes within roughly 20 ms (`MQTTstuff.ino:94-95`).

Independently, ADR-077 introduced a streaming HA-discovery drip that emits
discovery JSON at a steady cadence (default 3 s, throttled under heap pressure).
The drip is sized so a single discovery publish (300 to 1200 byte JSON body
plus lwIP pbufs) fits comfortably under the heap warning tier defined by
ADR-030.

When the two paths overlap, the cumulative pbuf and JSON-write allocation pushes
the heap into the WARNING tier even though neither path on its own would. Field
evidence from the Crashevans deployment (TASK-353) showed the
`iDripCooldownSkipCount` counter growing without discovery progressing under
heavy Status traffic: drip ticks were firing on top of bursts, getting throttled
by ADR-030, and skipping. The drip was effectively starving while the heap
indicator showed false-positive warnings.

The two existing layers do not solve this:

- ADR-052 (publish eligibility) decides *whether* a value is publishable. It
  does not decide *when in real time* a publish is safe to emit relative to
  another publish.
- ADR-030 (heap tier framework) decides whether the heap is healthy enough to
  publish at all. It is a reactive gate, not a coordination mechanism.

What was missing was an explicit timing layer that brackets the Status fanout
so the drip can stand aside while a burst is in flight, and stay clear long
enough for the lwIP pbufs from the burst to drain before the drip's own
allocation lands.

## Decision

MQTT publishers that fan out a Status frame must bracket the fanout with
`beginStatusBurst()` and `endStatusBurst()`, and the discovery drip loop must
defer for the duration of the window plus a `STATUS_BURST_COOLDOWN_MS` cooldown
bounded strictly below the Status-frame cadence.

The decision decomposes into four sub-rules. Each sub-rule is annotated with
its enforcement level per ADR-080.

### Sub-rule 1: Begin/end window (pattern-level binding)

Every `publish(Master|Slave)Status*State` function in `OTGW-Core.ino` must call
`beginStatusBurst()` before the first publish in the fanout and
`endStatusBurst()` after the last publish.

Enforcement: `check_status_publishers_wrap_burst` at `evaluate.py:957-1014`.
The gate parses each matching function body and fails the build if either
`beginStatusBurst(` or `endStatusBurst(` is absent.

### Sub-rule 2: Cooldown bound (pattern-level binding)

`STATUS_BURST_COOLDOWN_MS` must stay strictly below the typical Status-frame
cadence observed in production logs (~3 s, TASK-353). The hard rule encoded in
the gate is `< 3000` ms. The current value is 2000 ms (`MQTTstuff.ino:126`).

Values `>= 3000` are rejected unless one of the preceding 5 source lines
carries the literal marker `// verified tuning`. The marker is the only escape
hatch and exists so a deliberate retune backed by fresh log evidence can land
without disabling the gate.

Enforcement: `check_status_burst_cooldown_bound` at `evaluate.py:893-955`.

### Sub-rule 3: Drip-deferral (pattern-level binding)

`loopMQTTDiscovery()` must consult `isDripDeferred()` before issuing a
discovery drip publish. `isDripDeferred()` (`MQTTstuff.ino:164-172`) returns
true when either `isStatusBurstActive()` is true or the post-burst cooldown
window is still open.

Enforcement: a CI gate is being added in the same task that lands this ADR
(TASK-426). Until that gate lands, this sub-rule is enforced by code review
against `MQTTstuff.ino` only.

### Sub-rule 4: Timeout self-heal (guideline-level)

`isStatusBurstActive()` (`MQTTstuff.ino:146-154`) auto-clears the
`statusBurstActive` flag if more than `STATUS_BURST_TIMEOUT_MS` (currently
500 ms, `MQTTstuff.ino:125`) have elapsed since `beginStatusBurst()`. This
prevents an exception-path-missed `endStatusBurst()` from permanently freezing
the drip.

This is a defensive property of the implementation, not a pattern that
recurring code must follow. It is explicitly labelled guideline-level: it has
no CI gate and recurring code is not expected to reproduce this shape.

## Alternatives Considered

### Alternative 1: No-windowing baseline (let drip and Status-bursts overlap)

Skip the windowing entirely and rely on ADR-030's heap tier framework to
throttle the drip when allocation pressure rises.

Rejected. The Crashevans field log (TASK-353) showed the overlap pushes heap
into the WARNING tier unnecessarily, and `state.heapdiag.iDripCooldownSkipCount`
(then named `iDripSkipCount`) was observed growing without discovery
progressing. The reactive gate can detect the symptom but cannot prevent the
overlap that caused it. ADR-030 is a backstop, not a coordinator.

### Alternative 2: Mutex around every publish

Introduce a single MQTT publish lock that all publishes acquire before
emitting, serialising drip and Status traffic at the lock boundary.

Rejected for three independent reasons:

- It serialises *all* publishes, not just bursts versus drips. Publishes that
  do not conflict (e.g. an ad-hoc OT value publish unrelated to the Status
  fanout) pay latency for no benefit.
- It inverts the ESP8266 cooperative single-threaded model. There is no second
  thread to mutually exclude against, so the lock semantics would have to be
  reinvented on top of cooperative yield points, which is exactly the
  re-entrancy surface that produced the original problem.
- Re-entrant scenarios via `doBackgroundTasks()` (documented in
  `MQTTstuff.ino:1055`) would deadlock such a mutex on the second entry.

### Alternative 3: Larger lwIP TCP_SND_BUF

Tune the TCP send buffer upward so overlapping publishes can coexist at the
network layer, absorbing the burst before the application-layer pressure
shows up.

Rejected. RAM headroom on ESP8266 is the constraining resource (ADR-001,
ADR-053). A larger send buffer just defers fragmentation without solving it,
and the buffer has to coexist with HTTP, WebSocket, and Telnet on the same
lwIP heap. Trading discovery-drip stalls for HTTP/Telnet stalls is not a net
improvement, and the underlying coordination problem remains.

## Consequences

### Positive

- The protected timing contract is now nameable in code review and PR
  comments. Reviewers cite ADR-088, not the multi-paragraph inline comment at
  `MQTTstuff.ino:93-118`.
- Three of the four sub-rules are gate-enforced (one landing alongside this
  ADR), blocking accidental regression.
- The discovery drip and Status fanout coexist under heavy traffic without
  unnecessary heap pressure. Overlap-induced false WARNING-tier transitions
  are eliminated.
- The `iDripCooldownSkipCount` and `iEnteredWarningCount` counters become
  observable signals for tuning the cooldown if Status cadence changes in
  the field.

### Trade-offs

- The cooldown ceiling (`< 3000` ms) is tied to an empirically observed
  Status cadence (~3 s). If the cadence changes, for example because a
  firmware update changes PIC behaviour, the cooldown must be re-tuned and
  re-validated against fresh log evidence, not just lowered to fit.
- The timeout self-heal (sub-rule 4) is defensive, not contractual. It will
  mask a real bug (a missing `endStatusBurst()` on an exception path) by
  auto-clearing after 500 ms. The bug becomes visible only via
  `iEnteredWarningCount` regressions or careful log inspection.
- The `// verified tuning` escape hatch on sub-rule 2 is a one-line bypass of
  the gate. Reviewers must check that the marker is justified by fresh
  evidence, not just present.

### Risks and mitigations

- *Risk*: A new status publisher is added to `OTGW-Core.ino` without
  `beginStatusBurst()` / `endStatusBurst()`.
  *Mitigation*: `check_status_publishers_wrap_burst` at `evaluate.py:957-1014`
  fails the build by name match on `publish(Master|Slave)Status*State`.

- *Risk*: A tuner lowers the cooldown below ~200 ms in pursuit of more drip
  cycles per second, re-introducing burst-overlap regressions.
  *Mitigation*: post-tuning review must check `iDripCooldownSkipCount` and
  `iEnteredWarningCount` from a representative log. The comment block at
  `MQTTstuff.ino:107-115` documents this guidance for the next person who
  touches the constant.

- *Risk*: Discovery drip stalls indefinitely because `isDripDeferred()`
  returns true but `endStatusBurst()` never fires.
  *Mitigation*: sub-rule 4 (timeout self-heal) clears the burst flag after
  500 ms (`MQTTstuff.ino:149`); the cooldown then expires after 2000 ms
  (`MQTTstuff.ino:126`), and the drip resumes.

- *Risk*: The sub-rule 3 gate is delayed and the drip-deferral check is
  removed by accident in `loopMQTTDiscovery()` before the gate lands.
  *Mitigation*: TASK-426 lands the gate in the same change set as this ADR.
  Until then, code review on `MQTTstuff.ino` is the enforcement layer.

## Related Decisions

- **ADR-030** (Heap Memory Monitoring and Emergency Recovery): provides the
  heap-tier framework that this ADR cooperates with. Status-burst quiesce
  prevents drip allocations from compounding when heap is already in the
  WARNING tier.
- **ADR-052** (MQTT Publish Eligibility Contract): defines *whether* a publish
  is eligible. ADR-088 adds the *timing* layer on top, deciding when the
  Status fanout and the discovery drip are safe to overlap.
- **ADR-077** (Streaming MQTT HA Discovery Architecture): the discovery drip
  is the consumer of the deferral signal. Status-burst windowing is the
  producer side that lets ADR-077's streaming architecture coexist with
  bursty Status traffic without false heap warnings.
- **ADR-080** (Binding ADR Rules Must Have a CI Gate): this ADR satisfies
  ADR-080 from acceptance. Sub-rules 1 and 2 are gate-backed today
  (`evaluate.py:957`, `evaluate.py:893`), sub-rule 3 has its gate landing
  alongside this ADR (TASK-426), and sub-rule 4 is explicitly labelled
  guideline-level.

## References

- Implementation: `src/OTGW-firmware/MQTTstuff.ino:65-172` (begin/end window,
  cooldown, drip-deferral, timeout self-heal: the full contract block).
- Constants: `STATUS_BURST_TIMEOUT_MS = 500` at `MQTTstuff.ino:125`,
  `STATUS_BURST_COOLDOWN_MS = 2000` at `MQTTstuff.ino:126`.
- CI gates: `evaluate.py:893-955` (cooldown bound), `evaluate.py:957-1014`
  (publishers wrap burst). The third gate for sub-rule 3 is being landed
  alongside this ADR under TASK-426.
- Implementation history: TASK-342 (initial quiesce), TASK-347 (post-burst
  cooldown), TASK-353 (10000 to 2000 ms tuning under Crashevans log
  evidence), TASK-354 (VH wrap), TASK-371 (PIC PR-readout quiesce extension).
- ADR-080 governance: every binding rule in this ADR points to a gate;
  sub-rule 4 is explicitly guideline-level.
