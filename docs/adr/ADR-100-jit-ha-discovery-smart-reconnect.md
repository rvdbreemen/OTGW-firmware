# ADR-100: JIT HA Discovery with Smart Reconnect (Port of dev ADR-073)

## Status

Proposed, 2026-05-08. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

This ADR ports the decision from dev ADR-073 to the 2.0.0 worktree. The context and motivation are identical; see ADR-073 for full background. Short summary:

ADR-041 established the JIT discovery principle but left two gaps. In practice, the firmware shipped `markAllMQTTConfigPending()` in both `startMQTT()` and the `homeassistant/status → online` handler, causing all 256 OT discovery configs to be bulk-published at every boot — including IDs the boiler never uses.

Field evidence (Discord, 2026-05-08): telnet log confirmed all 256 configs published sequentially at boot.

### 2.0.0-specific note: broker restart heuristic not ported

On the dev branch (ADR-073 §3), the broker restart heuristic piggybacks on an existing `offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS` block in the MQTT connect success handler. That block does not exist in the current 2.0.0 `MQTTstuff.ino` — data republish on reconnect is handled unconditionally here. Porting the threshold check is a separate behavioural change and is deferred to a follow-up ADR.

Consequence: on 2.0.0, any MQTT reconnect (even a transient blip) leaves the done-bitmap intact. This is acceptable because the JIT trigger in `processOT()` will re-queue any MsgID whose done-bit gets cleared by a future bitmap reset.

### 2.0.0-specific note: firmware/PIC pseudo-IDs not yet present

`OTGWfwinfoid`, `OTGWpicinfoid`, and `OTGWpicsettingsid` are not defined on this branch. `publishNonOTDiscoveryConfigs()` therefore queues only: climate (ID 0), number (ID 27), Dallas sensors (`OTGWdallasdataid`), and heap stats (`OTGWheapstatsid`).

## Decision

Identical to ADR-073 §1, §2, §4, §5 — broker restart heuristic (§3) deferred.

### 1. Remove bulk publish from automatic triggers

- `startMQTT()`: `markAllMQTTConfigPending()` → `clearMQTTConfigDone()` + `clearMQTTConfigPending()` + `publishNonOTDiscoveryConfigs()`.
- `homeassistant/status → online` handler: `markAllMQTTConfigPending()` removed. No action. Broker retains discovery configs across HA restarts.

### 2. Non-OT discovery config set (`publishNonOTDiscoveryConfigs()`)

| Pseudo-ID constant | Entity |
|---|---|
| `0` | Climate: thermostat + DHW control |
| `27` | Number: outside temperature override |
| `OTGWdallasdataid` | Dallas temperature sensors |
| `OTGWheapstatsid` | Heap / discovery statistics |

`dripDeviceInfoPending = true` set alongside.

### 3. Broker restart heuristic

**Deferred.** The 2.0.0 connect success handler republishes unconditionally; adding `offlineMs` tracking is a separate change. Gap documented here; addressed in a follow-up ADR when the threshold check is ported.

### 4. Force path unchanged

`doAutoConfigure()` continues to call `markAllMQTTConfigPending()`. Intentional.

### 5. JIT trigger in `processOT()` unchanged

Already present and correct on this branch (lines 4087–4088 and 3546–3547).

### Trigger table (2.0.0 state)

| Trigger | Action |
|---|---|
| Boot / `startMQTT()` | `clearMQTTConfigDone()` + `clearMQTTConfigPending()` + `publishNonOTDiscoveryConfigs()`; OT IDs publish JIT |
| Top-topic changed | Same as boot |
| MQTT reconnect (any duration) | No action — bitmaps intact (broker restart heuristic deferred) |
| HA restart (`homeassistant/status → online`) | No action — broker retains configs |
| Force (REST or Serial `F`) | `markAllMQTTConfigPending()` — all IDs queued immediately |

## Alternatives Considered

See ADR-073 — identical reasoning. The only 2.0.0-specific alternative considered was porting the broker restart heuristic in the same commit; deferred because it requires separate `offlineMs` infrastructure that adds risk outside the narrow scope of this port.

## Consequences

### Positive

Same as ADR-073: no bulk publish at boot, HA entities appear JIT, HA restart is silent.

### Negative / Risks

- Broker restart leaves done-bitmap stale until a `startMQTT()` (top-topic change, explicit restart) clears it. Mitigated by: (a) `doAutoConfigure()` force path, (b) the deferred follow-up ADR.
- Firmware/PIC info entities not queued at boot (pending arrival of `OTGWfwinfoid` etc. on this branch).

## Related Decisions

- **Ports dev ADR-073** to 2.0.0 worktree. Decisions are coherent across both branches; implementation gap (broker heuristic) documented above.
- **Supersedes 2.0.0 ADR-041 equivalent** — the JIT principle from ADR-041 is preserved; the bulk-at-boot anti-pattern is removed.
- ADR-006, ADR-040, ADR-062, ADR-067 (dev numbering equivalents) — unaffected.

## References

- dev ADR-073: `docs/adr/ADR-073-jit-ha-discovery-smart-reconnect.md` (dev worktree).
- `src/OTGW-firmware/MQTTstuff.ino:568` — `startMQTT()` change (2.0.0 line number post-pull).
- `src/OTGW-firmware/MQTTstuff.ino:775` — `homeassistant/status` handler change.
- `src/OTGW-firmware/OTGW-Core.ino:4087-4088` — existing JIT trigger (unchanged).

## Enforcement

```json
{
  "llm_judge": true
}
```

LLM judge guidance: In `src/OTGW-firmware/MQTTstuff.ino`, verify: (a) `startMQTT()` does NOT call `markAllMQTTConfigPending()` directly — it must call `publishNonOTDiscoveryConfigs()` instead; (b) the `homeassistant/status → online` handler does NOT call `markAllMQTTConfigPending()` or `doAutoConfigure()` — any such call is a violation; (c) `doAutoConfigure()` (the force path) MAY call `markAllMQTTConfigPending()` — that is intentional. Flag any `markAllMQTTConfigPending()` call outside of `doAutoConfigure()` and `markAllMQTTConfigPending()` itself as a violation of the JIT principle.
