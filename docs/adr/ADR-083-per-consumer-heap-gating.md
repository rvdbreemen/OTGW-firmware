# ADR-083: Per-Consumer Heap Gating (WebSocket vs MQTT decoupling)

## Status

Proposed

Supersedes the single-ladder gating decision in ADR-030 (the four-level health system stays; this ADR changes *who* consults it).

## Context

The ESP8266 OTGW runs HTTP, WebSocket, MQTT and Telnet concurrently on ~40 KB RAM. ADR-030 introduced a four-level heap-health ladder (`getHeapHealth()` in `helperStuff.ino`) and adaptive backpressure to stop out-of-memory crashes. That decision routes **both** consumers through the **same** tier evaluation:

- `canSendWebSocket()` — `helperStuff.ino:989`
- `canPublishMQTT()` — `helperStuff.ino:1043`

Both call `getHeapHealth()` (`helperStuff.ino:940`), which maps free heap (with a `getMaxFreeBlockSize()` fragmentation promote, `helperStuff.ino:939`) to one shared `HeapHealthLevel`. Current thresholds are `HEAP_CRITICAL=1536`, `HEAP_WARNING=3072`, `HEAP_LOW=5120` (`helperStuff.ino:900-902`). Note these have drifted below the values ADR-030 documents (3072/5120/8192); the *single-ladder decision*, not the numbers, is what this ADR revisits.

**Field report (evidence).** George (`geo83_44083`), Discord `#beta-testing`, 2026-05-31: with the WebUI live-log (WebSocket OT-frame push) open, ESP8266 heap pressure rises and within ~10 min Home Assistant MQTT sensors go unavailable; closing the browser tab keeps MQTT rock-solid for hours. Maintainer (Rob) confirmed in-channel that the reliability changes "prioritize other things than the webui" and that with no logging running "the UI will become snappy" — i.e. the WebSocket live-log is the genuine heap/load driver. The MQTT short-write desync it exposes is fixed separately (TASK-769); this ADR addresses the *trigger*.

**The coupling is the problem.** The throttle *intervals* are already independent (`WEBSOCKET_THROTTLE_MS_WARNING=50` / `_CRITICAL=200` vs `MQTT_THROTTLE_MS_WARNING=100` / `_CRITICAL=500`, `helperStuff.ino:915-918`). What is shared is the **tier boundary**: both consumers cross from HEALTHY→LOW→WARNING→CRITICAL at the same free-heap values. Consequently, the obvious remedy for the field report — relax the MQTT gate so MQTT keeps publishing under pressure (TASK-779 AC#8) — cannot be done by loosening the shared ladder, because that *also* loosens the WebSocket throttle and makes the heap producer run hotter, worsening the very trigger. Producer and victim share one governor.

**Allocation asymmetry (the why behind producer/victim).** The live-log path is `sendLogToWebSocket()` → `webSocket.broadcastTXT(logMessage)` (`webSocketStuff.ino:264-267`). `broadcastTXT` copies the payload into a per-client send buffer, so each pushed OT frame costs up to `MAX_WEBSOCKET_CLIENTS` (3, `webSocketStuff.ino:59`) heap allocations plus WS framing. MQTT publishes are smaller, periodic, and HA-critical. So the two consumers are not symmetric: WebSocket is the dominant heap producer; MQTT is the heap-pressure victim whose unavailability the user actually notices.

## Decision

**Give each heap consumer its own threshold ladder while keeping a single shared CRITICAL out-of-memory floor.**

Split the tier decision so `canSendWebSocket()` and `canPublishMQTT()` no longer share LOW/WARNING boundaries:

1. **Shared CRITICAL floor — unchanged.** Both consumers stop completely below one common hard floor (today `HEAP_CRITICAL_THRESHOLD=1536`, plus the maxBlock fragmentation promote). This is the ADR-079 emergency-recovery safety net and is non-negotiable; decoupling never lets either consumer run below it. The fragmentation-aware `getMaxFreeBlockSize()` promote also stays.

2. **WebSocket ladder = stricter (producer self-limits earlier).** Because the live-log is the heap producer, its LOW/WARNING boundaries sit at or above the current shared values, so it begins backing off *before* it drives heap into the MQTT-desync band. WebSocket message loss under pressure is already accepted (ADR-030): a human watching a live log does not need every frame.

3. **WebSocket coalescing under pressure (drop-to-latest).** Below the WebSocket LOW boundary, the live-log coalesces rather than throttles blindly: it forwards only the most recent frame and discards intermediate frames it could not send, instead of queueing them. No new buffer is introduced (queues consume the heap we are protecting — ADR-030 Alternative 3 rejected for the same reason); coalescing reuses the existing fire-and-forget `broadcastTXT` path and simply skips frames. This caps the producer's allocation rate at the exact moment heap is scarce.

4. **MQTT ladder = relaxed (victim keeps publishing).** MQTT gets its own LOW/WARNING boundaries below the WebSocket ones, so it continues to publish/throttle-gently while the WebSocket path is already coalescing. The **specific relaxed MQTT threshold values are deferred to a follow-up** and MUST be set from real `logHeapStats` telemetry captured on George's device with the live-log open (TASK-779 AC#1/#8). This ADR fixes the *structure* (independent ladders, shared floor); it does not invent the numbers. The CRITICAL floor is excluded from any relaxation.

Mechanically this means introducing per-consumer threshold constants (e.g. `WS_HEAP_LOW/WARNING`, `MQTT_HEAP_LOW/WARNING`) and having each `can*` function evaluate its own tier, while both continue to honour the single `HEAP_CRITICAL_THRESHOLD`. `getHeapHealth()` remains available for observability (`logHeapStats`, REST `/health`) and for the CRITICAL decision.

## Alternatives Considered

### Alternative 1: Keep the single shared ladder, just relax all thresholds (do nothing structural)

**Pros:** Smallest diff; one place to tune.

**Cons:** Relaxing the shared ladder to help MQTT *also* relaxes the WebSocket throttle, making the heap producer run hotter and worsening the documented trigger. Directly contradicts the field evidence. **Rejected** — it optimises the victim by feeding the producer.

### Alternative 2: Per-consumer ladders with a shared CRITICAL floor (chosen)

**Pros:** Lets MQTT relax without loosening WebSocket; lets WebSocket back off earlier and coalesce; preserves the one hard OOM safety net; keeps `getHeapHealth()` for diagnostics. Matches the producer/victim asymmetry the allocation analysis shows.

**Cons:** Two ladders to reason about instead of one; a few more `#define`s; tuning now has two axes. Accepted as the cost of correctness.

### Alternative 3: Queue/buffer WebSocket frames and drain when heap recovers

**Pros:** No frame loss; smooths bursts.

**Cons:** A queue consumes the very heap under protection; overflow still drops; adds latency even when healthy. Already rejected in ADR-030 (Alternative 3) for ESP8266; drop-to-latest coalescing achieves the backpressure goal with zero added buffer. **Rejected.**

### Alternative 4: Disable the live-log entirely under any heap pressure

**Pros:** Trivially removes the producer.

**Cons:** Too blunt — the live-log is a primary diagnostic surface, and a binary on/off discards the graceful-degradation property ADR-030 deliberately built. Coalescing keeps a usable (if thinned) log instead of a black screen. **Rejected.**

## Consequences

### Positive

- Relaxing the MQTT gate (TASK-779 AC#8) becomes possible without making the WebSocket trigger worse: the two are no longer the same knob.
- The WebSocket live-log self-limits earlier and coalesces, so it cannot drive heap into the MQTT-desync band as readily (addresses TASK-779 AC#2/#3).
- The single CRITICAL OOM floor and ADR-079 emergency recovery are preserved unchanged — no regression to the crash-prevention guarantee.
- `getHeapHealth()` and all heap diagnostics keep working; the change is additive at the decision layer.

### Negative

- Two threshold ladders increase configuration surface and the chance of mis-tuning one relative to the other. Mitigation: derive both from the same telemetry pass; keep the shared CRITICAL floor as the invariant.
- Coalescing means a user watching the live-log under heap pressure sees gaps (only latest frames). Accepted: thinned log >> stalled MQTT; consistent with ADR-030's accepted-message-loss stance.
- The relaxed MQTT numbers cannot be finalised in this ADR; they remain a telemetry-gated follow-up, so the decoupling lands structurally before it lands quantitatively.

### Risks & Mitigation

- **Risk:** WebSocket ladder set too strict makes the live-log feel dead even on healthy heap. Mitigation: only the LOW/WARNING boundaries move; HEALTHY behaviour (full speed) is unchanged, and boundaries are `#define`s tuned from telemetry.
- **Risk:** MQTT ladder relaxed too far reintroduces OOM. Mitigation: the shared CRITICAL floor is excluded from relaxation and is the hard backstop; ADR-079 recovery still fires below it.

## Related Decisions

- **ADR-030** — Heap monitoring and emergency recovery (single-ladder gating; superseded by this ADR for the gating-ownership decision, four-level system retained).
- **ADR-079** — Emergency heap recovery actions (the CRITICAL-floor safety net this ADR keeps shared).
- **ADR-076** — Drop global MQTT rate gate on status fan-out (prior MQTT-side backpressure tuning).
- **TASK-779** — Make WebSocket live-log connection more reliable under heap pressure (drives this ADR; AC#1/#2/#3/#8/#9).
- **TASK-769** — MQTT short-write desync fix (the symptom this trigger exposes; fixed separately).
- Code: `helperStuff.ino` `getHeapHealth()` (940), `canSendWebSocket()` (989), `canPublishMQTT()` (1043), thresholds (900-902, 915-918); `webSocketStuff.ino` `sendLogToWebSocket()` (264), `MAX_WEBSOCKET_CLIENTS` (59); `OTGW-firmware.h` `HEAP_LOW_RESTORE_THRESHOLD` (115).

## References

- Discord `#beta-testing`, George (`geo83_44083`) + maintainer (Rob), 2026-05-31 — field report: live-log open → MQTT unavailable in ~10 min; tab closed → stable for hours.
- ESP8266WebSockets library `broadcastTXT()` — per-client payload copy (per-frame heap cost × `MAX_WEBSOCKET_CLIENTS`).
- ADR-030 §Decision and §Alternatives (single-ladder design and the rejected queue alternative reused here).
