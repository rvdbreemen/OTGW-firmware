# ADR-121: Per-Consumer Heap Gating for the WebSocket Live-Log vs MQTT Publish

## Status

Accepted (Option B), 2026-06-02. Decision Maker: Robert van den Breemen.

Classification: **binding** amendment to ADR-089. Option B adds per-consumer heap
thresholds, which changes the tier contract, so per ADR-080 this ADR MUST have a
CI gate: `check_per_consumer_heap_gate` in `evaluate.py`. That gate and the Option
B implementation land together under TASK-779 and are a committed
Definition-of-Done item of that work, not optional. The concrete WS-strict /
MQTT-relaxed threshold *values* are telemetry-gated (George's `logHeapStats` on
the failing scenario, AC#8) and will be set in the implementation commit; the
*structure* (two independent ladders) is the decision recorded here.

## Status History

status_history:
  - date: 2026-06-02
    status: Proposed
    changed_by: Claude (TASK-779)
    reason: Records the corrected diagnosis (the dev WebSocket heap-gate was never ported to 2.0.0, leaving the live-log ungated) and proposes the per-consumer gating model for maintainer decision. The parity-restoring gate port shipped in the same commit; the decouple options + telemetry-driven MQTT values are deferred to Accept.
    changed_via: adr-kit
  - date: 2026-06-02
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer selected Option B (independent per-consumer heap thresholds) so relaxing the MQTT gate cannot loosen the WebSocket gate. Binding amendment to ADR-089; CI gate check_per_consumer_heap_gate + the implementation land under TASK-779. Concrete WS-strict/MQTT-relaxed values are telemetry-gated (George's logHeapStats, AC#8).
    changed_via: adr-kit

## Context

George (geo83_44083) reported in Discord #beta-testing on 2026-05-31: with the
web UI live-log (the WebSocket OT-frame push) open, an ESP8266 gateway loses its
Home Assistant MQTT sensors (they go unavailable) within roughly ten minutes;
closing the browser tab keeps MQTT rock-solid for hours. The live-log is the
heap-pressure trigger.

TASK-779 was originally framed on this premise (quoted from the task):

> "getHeapHealth() in helperStuff.ino feeds ONE tier ladder gating both
> canSendWebSocket() and canPublishMQTT(); WS live-log is the heap hog."

That premise is **factually wrong on the 2.0.0 branch**, and the corrected
diagnosis is the reason this ADR exists:

- `canSendWebSocket()` is **dead code** on 2.0.0. It is declared
  (`OTGW-firmware.h:168`) and fully defined (`helperStuff.ino:939`, with its own
  per-tier throttle using `WEBSOCKET_THROTTLE_MS_WARNING`/`_CRITICAL`), but a
  repo-wide grep finds **zero callers**.
- The live-log push path `sendLogToWebSocket()` (`webSocketStuff.ino:265`, called
  from five OT-hot-path sites in `OTGW-Core.ino` and `OTDirect.ino`) calls
  `webSocket.broadcastTXT()` with **no heap gate at all**.
- Therefore, on 2.0.0, the WebSocket live-log broadcasts **every** OT frame to
  every connected client unconditionally, while only MQTT is gated
  (`canPublishMQTT()`). The WS path is the unthrottled heap consumer; MQTT is the
  victim that hits the shared low-heap tiers and drops.

Git archaeology explains why: the WebSocket gate was deliberately added on the
`dev` branch (commit `34e55dcf` "Optimize logging: cache OT timestamp, gate
WebSocket, throttle flush", 2026-04-12), and `dev` still carries
`&& canSendWebSocket()` in `sendLogToWebSocket()`. The 2.0.0 form of that line
predates the dev fix (blame `6a7f83b58`, 2026-03-06) and the fix was **never
ported** to 2.0.0. The WS throttle constants are identical on both branches
(`WEBSOCKET_THROTTLE_MS_WARNING=50`, `WEBSOCKET_THROTTLE_MS_CRITICAL=200`) and
have run in dev production for ~7 weeks.

There are two distinct problems, and they must not be conflated:

1. **Parity regression (now fixed):** the WS live-log was ungated on 2.0.0. The
   commit that carries this ADR restores `&& canSendWebSocket()` to
   `sendLogToWebSocket()`, matching dev. This gives the live-log the heap
   backpressure it was missing.
2. **The coupling that the maintainer originally wanted to solve (open):** once
   the gate is wired, both `canSendWebSocket()` and `canPublishMQTT()` consult the
   **same** `getHeapHealth()` tier ladder. So if a future change relaxes the MQTT
   side by loosening the shared threshold ladder (TASK-779 AC#8, to keep HA
   sensors available longer), it would **also** loosen the WS gate, letting the
   live-log run hotter again, worsening the very trigger. The per-tier throttle
   *intervals* are already independent (separate constants + timestamps); the
   *tier thresholds* are shared. Decoupling the thresholds is the open design
   question this ADR frames.

## Decision

**Accepted: Option B (independent per-consumer heap thresholds).** The WebSocket
live-log gate and the MQTT publish gate each get their own threshold ladder, so
relaxing the MQTT side can never loosen the WS side (the coupling that defeated
the original single-ladder approach). The parity floor (the restored gate)
already shipped in alpha.139; Option B is the decouple layered on top of it.

**Shipped (floor):** `sendLogToWebSocket()` now calls `canSendWebSocket()`,
restoring 2.0.0 to dev's gating behaviour. This is a cross-branch port of a
proven fix, not a new design, and it closes no gated acceptance criterion of
TASK-779.

**Decided (Option B):** the WS gate and the MQTT gate use independent threshold
ladders instead of the shared `getHeapHealth()` tiers, so relaxing the MQTT side
(AC#8) cannot loosen the WS side. Option B was chosen over the other candidates
(see Alternatives Considered) by the maintainer on 2026-06-02. The implementation
adds the per-consumer threshold sets plus the `check_per_consumer_heap_gate`
CI gate, under TASK-779.

The concrete relaxed-MQTT / stricter-WS threshold **values** are deliberately
**not fixed in this ADR**: they require real `logHeapStats` telemetry captured on
a gate-restored build (alpha.139+) so the numbers reflect the post-fix heap
profile, not the ungated one. Choosing values from pre-fix logs would re-baseline
against the bug this ADR removes. The implementation therefore lands in two
steps: (1) the independent-ladder structure (behaviour-equivalent to today until
values diverge), (2) the value tuning once George's telemetry is in. Step 1 may
ship before step 2; step 2 must not be guessed.

## Alternatives Considered

1. **Option A — keep the single shared ladder (as shipped).** The restored gate
   uses the existing `getHeapHealth()` tiers for both consumers. Pro: zero new
   surface, already running, matches dev exactly. Con: does not solve the
   coupling — relaxing the shared thresholds for MQTT (AC#8) would re-loosen the
   WS gate. Acceptable only if AC#8 is achieved by tuning the MQTT *throttle
   intervals* (already independent) rather than the shared *thresholds*.
2. **Option B — independent per-consumer thresholds.** Give the WS gate a
   stricter threshold ladder (back off earlier, it is the heap hog) and the MQTT
   gate a more lenient one (stay available longer), so relaxing MQTT never
   loosens WS. Pro: directly solves the coupling; matches the maintainer's stated
   intent. Con: doubles the threshold surface and the ADR-089 tier contract,
   needs a CI gate, and the two ladders must be kept ordered/consistent (new
   `evaluate.py` check). Threshold values still telemetry-pending.
3. **Option C — WS-side coalescing / drop-to-latest.** Instead of (or on top of)
   threshold tuning, bound the live-log's allocation rate by coalescing frames
   under pressure (e.g. keep only the newest pending line, or rate-cap to N/sec
   independent of heap tier). Pro: attacks the allocation volume directly, which
   is the root cost; degrades the live-log gracefully rather than cutting it.
   Con: more code than a threshold; the current `broadcastTXT` path has no queue
   to coalesce in (the "Simplified: no queue" path would have to grow one back).
4. **Option D — do nothing beyond the port.** Ship only the parity fix and close
   TASK-779's decouple ACs as won't-do. Rejected as the *default* because the
   coupling the maintainer identified is real and will resurface the moment AC#8
   is attempted; but listed because the port alone may already resolve George's
   symptom, in which case the decouple could be deferred to a later milestone on
   its own merits rather than forced now.

## Consequences

**Positive:**

- The shipped floor restores heap backpressure to the WS live-log on 2.0.0: under
  `HEAP_CRITICAL` it is blocked, under `WARNING`/`LOW` it is throttled, matching
  dev. The unthrottled-broadcast trigger is removed.
- Shipping it under its own prerelease tag gives field testers a clean A/B
  artifact (alpha.138 ungated vs alpha.139 gated) to confirm whether the gate
  alone resolves the MQTT-unavailable symptom before any MQTT-side change lands.
- The corrected diagnosis is now on record, so the decouple is designed from the
  true starting state (WS was ungated) rather than the wrong premise.

**Negative / trade-offs:**

- The floor (Option A behaviour) leaves the WS and MQTT gates sharing one tier
  ladder, so the coupling problem is documented but not yet solved; attempting
  AC#8 via shared-threshold relaxation without first adopting Option B/C would
  re-introduce the trigger.
- The ESP32-S3 (OTGW32) path now also runs the gate. On ESP32 the abundant heap
  means `getHeapHealth()` rarely leaves `HEAPHEALTHY`, so the gate is near-always
  open and the behavioural change there is negligible; this is a noted asymmetry,
  not a regression.

**Risks and mitigations:**

- *Risk:* the live-log dies too eagerly at mild pressure, hurting the debugging
  UX. *Mitigation:* the throttle constants are the dev-proven values; if field
  reports show over-eager cutoff, tune `WEBSOCKET_THROTTLE_MS_*` (interval, not
  threshold) — that does not touch the shared ladder.
- *Risk:* choosing relaxed MQTT values from pre-fix telemetry. *Mitigation:*
  values are deferred until `logHeapStats` is captured on the gate-restored
  build.

## Related Decisions

- **ADR-030** (Heap Memory Monitoring and Emergency Recovery): defines the
  4-state heap health model and the publish-throttling consumer pattern. This ADR
  amends it **by reference only** (ADR-030 is immutable; not edited here) by
  proposing per-consumer gating on top of the shared model.
- **ADR-089** (Heap Tier Machine Contract): the binding, CI-gated tier ladder
  (`check_heap_tier_thresholds_ordered`, fragmentation promotion, tier-entry
  counters). Option B would extend this contract with a second per-consumer
  ladder and require a companion CI gate; amended by reference, not edited.
- **ADR-080** (binding ADRs must have a CI gate): governs the classification note
  in Status — the Accept decision must declare binding-with-gate or
  guideline-level.

## References

- TASK-779 (this work; corrected diagnosis in its plan + notes).
- Dev origin of the gate: commit `34e55dcf` ("Optimize logging: ... gate
  WebSocket ...", 2026-04-12), still present in `dev:src/OTGW-firmware/webSocketStuff.ino`.
- 2.0.0 ungated form: blame `6a7f83b58` (2026-03-06), `webSocketStuff.ino:265`.
- Code paths: `sendLogToWebSocket()` (`webSocketStuff.ino:265`),
  `canSendWebSocket()` (`helperStuff.ino:939`), `canPublishMQTT()`
  (`helperStuff.ino:993`), `getHeapHealth()` (`helperStuff.ino:892`).
- Field report: George (geo83_44083), Discord #beta-testing, 2026-05-31. Board
  confirmed ESP8266; exact model (NodeMCU v3) not yet confirmed in the
  transcript — confirm from his banner when he next reports.
- Telemetry dependency: AC#8 relaxed MQTT values require `logHeapStats` from a
  gate-restored build (alpha.139+).
