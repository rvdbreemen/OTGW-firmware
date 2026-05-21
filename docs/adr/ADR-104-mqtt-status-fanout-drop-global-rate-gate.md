# ADR-104: MQTT Status Fan-Out — Drop Global Rate-Gate, Keep Per-Slot Heartbeat (2.0.0)

## Status

Accepted, 2026-05-21.

## Context

This ADR is the 2.0.0 sibling of dev's **ADR-076**. The decision is the same; the worktrees keep independent numbering. Both ADRs must remain coherent in intent so the 1.5.x and 2.0.0 lines do not diverge on this contract.

ADR-052 defined the MQTT publish eligibility contract for status topics as **first-seen OR value-changed OR stale-refresh**, with the stale-refresh threshold interpreted as a *maximum stale age per slot* — explicitly **not** a minimum spacing between any two publishes (ADR-052, Alternative 1, rejected).

After ADR-052 was accepted, the same gating model was extended to other high-frequency fan-out sites — msgId 5 (ASF), msgId 6 (RBP), msgId 100 (Remote Override) — via TASK-401, and the stale-refresh threshold was pinned at a hardcoded `STATUS_HEARTBEAT_INTERVAL_SEC = 60` seconds (independent of `settings.mqtt.iInterval`) for the Status fan-out. So far so good: each slot has its own `lastPublished` timestamp and refreshes once per minute when no change occurred.

TASK-402 then added a **global** rate-gate on top of the per-slot heartbeat:

```cpp
// src/OTGW-firmware/OTGW-Core.ino:270-271
static uint32_t mqttLastGatedPublishMs = 0;
static constexpr uint32_t MQTT_GATED_PUBLISH_SPACING_MS = 250;
```

The motivation was heap pressure: each msgId 0 / 70 frame can fan out into ~18 publishes (2 byte topics + 16 bit topics) inside a single `processOT()` call, and the broader fan-out across msgId 0 / 70 / 5 / 6 / 100 sums to ~50 gated slots. The 250 ms global token was meant to spread the recurring 60-second heartbeat storm across ~4 s instead of letting all slots fire back-to-back. Change-detect was explicitly exempted (bit-flips publish immediately), and a refinement under TASK-612 also exempted first-seen / forced bursts so newly observed slots would not be starved at boot.

The follow-up bug report from beta testing (filed against the 1.5.x dev line first, but the same code path exists here on 2.0.0) showed the regime was still broken. Specific symptoms:

- Bits 2–5 of `OT_Statusflags` consistently entered HA as `unknown` after a clean boot and stayed there across the 60 s heartbeat boundary.
- The combined `status_master` byte topic always published on the first heartbeat tick of the new minute.
- The per-bit slots in the same byte intermittently did not.
- Slots that did publish under a heap-throttled `sendMQTTData()` ended up with their `lastPublished` updated to "now" anyway, suppressing the next heartbeat.

Root-causing the symptoms surfaced two distinct defects (identical in shape on dev and 2.0.0).

### Defect 1 — Per-slot starvation under the global gate

The 250 ms token is a single global. Inside one `processOT()` call, the print order through `print_ASFflags` evaluates the combined byte slot first, then the eight bit slots. On a heartbeat tick, all nine slots are simultaneously `intervalElapsed && !forcePublish && !firstSeen`. The byte slot grabs the token (`mqttLastGatedPublishMs = nowMs`); the eight bit slots all see `(nowMs - mqttLastGatedPublishMs) < 250 ms` and return `false`. The next heartbeat fires 60 s later; the byte slot starts the new minute first and wins the token again. The bits never get a 60 s window when no other slot in the same fan-out has just published.

This is the inverse of what the gate was supposed to do: instead of spreading the storm over 4 s, it concentrated all the publishes into the byte slot and starved the bit slots indefinitely.

It also violates ADR-052 in spirit. ADR-052 said: *"If multiple bits change in the same status frame, all affected per-bit topics publish from that one frame."* The global gate suppresses the heartbeat equivalent of that statement — *"if multiple slots stale-refresh in the same frame, all of them publish from that one frame."*

### Defect 2 — Stale pending committed by an unrelated publish

`shouldPublishTrackedStatusBit()` and `shouldPublishTrackedStatusByte()` install a `mqttPendingBitSlot` / `mqttPendingByteSlot` record describing the slot that is about to publish, then return `true`. The caller (`publishStatusBitMQTT`, etc.) invokes `sendMQTTData()`. On the success path, `sendMQTTData()` calls `confirmMQTTPublishBitSlot()` / `confirmMQTTPublishByteSlot()` to commit the pending record into the per-slot tracked-time array.

If `sendMQTTData()` early-returns — because `canPublishMQTT()` says the heap is too low (ADR-089 / ADR-030), or `MQTTclient.connected()` is false, or any other guard fires — the pending record stays live with `pending = true`. The next *successful* `sendMQTTData()` call for any topic anywhere in the firmware then runs the same `confirmMQTTPublishBitSlot/ByteSlot` calls and commits that stale pending into the slot's tracked-time array. The slot is now marked as "published at time T" even though HA never received the value, and the per-slot heartbeat predicate `elapsedTrackedSeconds(now, lastTime) >= 60` is false until 60 s after the failed attempt.

The same defect shape also affects `mqttPendingSlot` (the normal-msgId pending) and the PS=1 path: `shouldPublishMQTTForID()` and `shouldPublishMQTTForPSField()` install pending inside an `OTPublishGate` block; multiple `sendMQTTData()` calls may follow inside the same block (base topic + source-separated `/thermostat` or `/boiler` + flag8 expansions); if all of them fail under heap pressure, an unrelated successful `sendMQTTData()` later silently commits the stale pending. This extension is scoped to this ADR as well (Decision item 7).

Both defects compound: the gate starves per-bit slots even when the heap is healthy, and when an attempt does happen under heap pressure the failure is silently masked as success.

### Constraints

- **ESP8266 / ESP32-S3 heap-tier semantics:** heap-tier back-pressure (`canPublishMQTT()` / `getHeapAvailableTier()`, ADR-030 + ADR-089) must remain the dominant guard against publish bursts overflowing the MQTT client buffers. On ESP32-S3 the absolute heap headroom is larger than on ESP8266, but the per-tier contract is identical and the publish path is shared.
- **ADR-052 contract:** per-slot first-seen / value-changed / stale-refresh semantics are externally visible to Home Assistant and must not regress.
- **Cross-line coherence with dev:** dev's ADR-076 is the parallel decision on the 1.5.x line. The two ADRs are authored as a coherent pair; the wording and Decision items are intentionally parallel even though file paths and line numbers differ slightly between branches.
- **No retained discovery deprecation:** this ADR makes no change to MQTT retained behaviour; retained discovery, retained availability (ADR-102 sibling of dev's ADR-074), and non-retained state semantics remain as they are.

## Decision

**Remove the global rate-gate from the MQTT status fan-out. Per-slot heartbeat + heap-tier back-pressure are the only throttles. Scope every pending-slot commit (bit, byte, and normal-msgId) to the publish that installed it.**

Concretely (line numbers reference the current 2.0.0 source at `feature-dev-2.0.0-otgw32-esp32-sat-support`):

1. **Delete** `static uint32_t mqttLastGatedPublishMs` and `static constexpr uint32_t MQTT_GATED_PUBLISH_SPACING_MS` from `OTGW-Core.ino` (lines 270-271).
2. **Delete** the two `if (intervalElapsed && !forcePublish && mqttLastGatedPublishMs != 0 && (nowMs - mqttLastGatedPublishMs) < MQTT_GATED_PUBLISH_SPACING_MS) return false;` blocks inside `shouldPublishTrackedStatusBit()` and `shouldPublishTrackedStatusByte()`.
3. **Delete** the `mqttLastGatedPublishMs = nowMs;` updates that follow, and the `mqttLastGatedPublishMs = 0;` reset inside `resetMqttTrackedState()` (line 418).
4. **Keep** `STATUS_HEARTBEAT_INTERVAL_SEC = 60` and the `intervalElapsed` predicate exactly as they are. The 60 s per-slot heartbeat remains the sole stale-refresh mechanism for the status fan-out.
5. **Keep** the change-detect priority path and the first-seen / forced one-shot path exactly as they are.
6. **Fix the stale-pending defect for bit/byte slots** by scoping pending commits to the publish that installed them: the bit/byte confirmation calls move out of `sendMQTTData()` into the immediate publish helper (`publishStatusBitMQTT` / `publishStatusByteMQTT` and their ASF/RBP/RO/VH siblings). The new contract is: *pending is installed → publish helper calls `sendMQTTData()` → if the publish succeeds, the helper calls `confirmMQTTPublishBitSlot/ByteSlot()` itself; otherwise it clears `.pending = false`*. `sendMQTTData()` returns `bool` to signal success.
7. **Fix the stale-pending defect for normal-msgId slots** with the same shape, adapted for the multi-publish case. `sendMQTTData()` increments a global monotonic `mqttSendSuccessCount` on every confirmed publish. The two `OTPublishGate` callers (`shouldPublishMQTTForID` site in the OT decode path, and `shouldPublishMQTTForPSField` site in the PS=1 summary path) capture `mqttSendSuccessCount` before the gate, run the publish block, and after the block either call `confirmMQTTPublishSlot()` (if the counter advanced) or clear `mqttPendingSlot.pending = false`. `confirmMQTTPublishSlot()` is removed from `sendMQTTData()`.

After the change, the publish predicate for every gated slot reduces to:

$$
publish = firstSeen \lor valueChanged \lor (now - lastPublished \ge 60s)
$$

— exactly the per-slot contract ADR-052 established, with no global override. All three pending-slot types (bit, byte, normal-msgId) follow the same rule: a pending record can only be committed by the publish helper that installed it; an unrelated `sendMQTTData()` cannot silently advance a slot's `lastPublished` timestamp.

### Why this choice

- It is the simplest predicate that resolves the user-visible regression. Bits 2–5 entering HA as `unknown` are gone because each per-bit slot is judged on its own clock, never against a sibling's publish timestamp.
- It returns the firmware to ADR-052's stated contract for the status fan-out instead of layering a quietly-different model on top.
- It removes an entire piece of state (`mqttLastGatedPublishMs`) and two branches of fan-out logic — net negative diff in `OTGW-Core.ino`, no new state added except the small `mqttSendSuccessCount` counter required for the multi-publish normal-msgId path.
- It does not blunt heap protection. `canPublishMQTT()` already throttles publishes when free heap drops below the configured tier (ADR-089 on 2.0.0, ADR-030 cross-line); that mechanism is unchanged and remains the only back-pressure the fan-out needs.

### Acknowledging the iteration

This ADR is the third pass on the status fan-out and explicitly closes the loop on TASK-401 / TASK-402 / TASK-612 on this branch:

- TASK-401 introduced per-slot heartbeat for ASF / RBP / RO. **Kept** — that was correct.
- TASK-402 added the 250 ms global rate-gate to spread the heartbeat storm. **Reverted** — the cure was worse than the disease, because the gate is a global token and the heartbeat tick is a synchronised event across all slots.
- TASK-612 exempted first-seen / forced bursts from the gate. **Subsumed** — with the gate gone there is no exemption to maintain.

The lesson recorded for future work: cross-slot global spacing is not a valid back-pressure mechanism for the status fan-out, because the heartbeat is intrinsically synchronised across slots that share an `intervalElapsed` boundary. Heap-tier back-pressure (per-publish, queue-aware, ADR-089 / ADR-030) is the only throttle that composes with the per-slot eligibility contract.

## Alternatives Considered

The same alternatives apply on 2.0.0 as on dev. They are documented in full in ADR-076 (dev's parallel decision); summarised here:

### Alternative 1: Keep the 250 ms gate, randomise per-slot phase to break the lockstep

Adds per-slot phase state and a deterministic-but-non-obvious schedule. Still violates ADR-052's per-slot independence — a slot that is genuinely stale at heartbeat-tick + ε can still be blocked because an unrelated slot won the token at heartbeat-tick. Does not address the stale-pending defect. **Rejected.**

### Alternative 2: Replace the global gate with a per-slot minimum spacing

For the status fan-out, the only republish path under steady state is the 60 s heartbeat itself — a per-slot minimum spacing on top of that is either silently inactive (spacing < 60 s) or actively breaks ADR-052 (spacing > 60 s). Adds 50+ × `uint32_t` of state for a guard that never fires. **Rejected.**

### Alternative 3: Drop the 60 s heartbeat instead of the rate-gate

Re-opens the recovery problem ADR-052 explicitly closed: long-lived stable binary sensors look stale or unknown from the HA side if discovery / retained state was lost between boot-up and the last value change. Trades a measured-and-fixable bug for an under-specified recovery posture. **Rejected.**

### Alternative 4: Accept the regression and document it

Field testers on Discord have already flagged the symptom on the dev (1.5.x) line. The bug is fixable; documenting around it is not the answer. The 2.0.0 line carries the same defect inherited from the shared OTGW-Core code. **Rejected.**

## Consequences

### Positive

- **Bits 2–5 publish on heartbeat.** Per-bit slots reach their own 60 s boundary independently of the combined byte slot. Each slot is judged on its own `lastPublished`, never against a sibling's.
- **Stale-pending defect closed for all three slot types.** A heap-throttled `sendMQTTData()` no longer leaves a dirty pending record that some unrelated publish later commits. Pending records are confirmed only by the publish helper (bit/byte) or `OTPublishGate` caller (normal-msgId) that installed them.
- **ADR-052 holds intact for the status fan-out.** No new contract; the existing contract now applies uniformly.
- **Less state.** `mqttLastGatedPublishMs` is gone. `MQTT_GATED_PUBLISH_SPACING_MS` is gone. Two `if`-blocks plus their timer updates are gone. Net add: one `uint32_t mqttSendSuccessCount` counter for the multi-publish normal-msgId path.
- **Coherent with dev.** ADR-076 and ADR-104 codify the same decision; the implementation diffs on dev and 2.0.0 are structurally identical.

### Negative

- **Heartbeat burst is sharper.** On a 60 s heartbeat tick, up to ~18 publishes for a single msgId fan-out happen back-to-back inside one `processOT()` call (≈ a few ms of MQTT TX), versus being spread over ~4 s by the gate. `canPublishMQTT()` still throttles per-publish under heap pressure, so the worst case degrades gracefully. Empirically the fan-out per parent msgId is ≤ 18 publishes × ~80 bytes, well within the MQTT client's TX buffer on both ESP8266 and ESP32-S3.
- **Behavioural diff visible to field testers.** Existing alpha users on 2.0.0 will see bits 2–5 start reporting on heartbeat for the first time. This is correct, but appears as a behaviour change.

### Risks & Mitigation

- **Risk:** A future fan-out site adds more slots and the heartbeat-tick burst grows beyond the heap-tier safety margin.
  **Mitigation:** `canPublishMQTT()` is the documented back-pressure point (ADR-089 / ADR-030); future fan-out work that meaningfully grows the per-frame slot count should be evaluated against the heap-tier thresholds, not by reintroducing a global spacing token.

- **Risk:** Someone reads the deletion of `MQTT_GATED_PUBLISH_SPACING_MS` as licence to remove all spacing from MQTT.
  **Mitigation:** This ADR is scoped to the status fan-out and the normal-msgId pending. Non-status throttles governed by `settings.mqtt.iInterval` are out of scope and unchanged.

- **Risk:** The stale-pending fix moves confirmation into helpers that don't all exist (some bit/byte topic publishes might go through a path that doesn't currently own the pending lifecycle).
  **Mitigation:** Implementation tasks audit every caller of `shouldPublishTrackedStatusBit/Byte` and `shouldPublishMQTTForID/PSField` and confirm each goes through one of the relocated commit sites. Verified by grep that `confirmMQTTPublishBitSlot/ByteSlot/Slot` are called only from those sites after the change.

## Related Decisions

- **ADR-076 (dev's parallel ADR):** Same decision on the 1.5.x line. Authored and Accepted as a coherent pair.
- **ADR-052:** MQTT Publish Eligibility and Reconnect Refresh Contract — this ADR re-affirms ADR-052's per-slot semantics and explicitly closes the door on global cross-slot spacing as a back-pressure mechanism for the status fan-out.
- **ADR-030:** Heap Memory Monitoring and Emergency Recovery — heap-tier back-pressure via `canPublishMQTT()` remains the sole publish throttle for the status fan-out after this change.
- **ADR-089:** Heap-Tier Machine Contract — the 2.0.0-specific tier contract governing `canPublishMQTT()` on ESP8266 and ESP32-S3.
- **ADR-006:** MQTT Integration Pattern — overall MQTT architecture; unchanged.
- **ADR-038:** OpenTherm Data Flow Pipeline — describes the synchronous `processOT()` fan-out path that materialises the heartbeat burst; unchanged.
- **ADR-044:** Global State — `extern` Declaration Pattern — the deleted globals lived under this pattern; `mqttSendSuccessCount` is added under the same pattern.
- **ADR-102:** HA Availability Reflects MQTT Link, Not OT Bus — unaffected; LWT / retained availability semantics are separate from the gated state-publish path.

## References

- Implementation area: `src/OTGW-firmware/OTGW-Core.ino` (rate-gate at lines 270-271; commit logic at lines ~1420-1442, ~1502, ~1542)
- Implementation area: `src/OTGW-firmware/MQTTstuff.ino` (sendMQTTData success paths around lines 1222-1224 and 1266-1268 — confirmation calls being relocated)
- Implementation area: `src/OTGW-firmware/OTGW-firmware.h` (sendMQTTData declarations changing from void to bool)
- Prior backlog tasks: `backlog/tasks/task-401`, `backlog/tasks/task-402`, `backlog/tasks/task-612` (where present on this branch)

## Enforcement

```json
{
  "llm_judge": true
}
```
