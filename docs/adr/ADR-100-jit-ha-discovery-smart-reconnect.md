# ADR-100: JIT HA Discovery with Smart Reconnect (Port of dev ADR-073)

## Status

Proposed, 2026-05-08. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

This ADR ports the decision from dev ADR-073 to the 2.0.0 worktree. The context and motivation are identical; see ADR-073 for full background. Short summary:

ADR-041 established the JIT discovery principle but left two gaps. In practice, the firmware shipped `markAllMQTTConfigPending()` in both `startMQTT()` and the `homeassistant/status → online` handler, causing all 256 OT discovery configs to be bulk-published at every boot — including IDs the boiler never uses.

Field evidence (Discord, 2026-05-08): telnet log confirmed all 256 configs published sequentially at boot.

### 2.0.0-specific note: broker restart heuristic ported (gap resolved 2026-05-08)

The broker restart heuristic from ADR-073 §3 is now implemented on this branch. The 2.0.0 connect success handler previously republished unconditionally; it now uses the same `offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS` guard as dev. The required infrastructure (`iLastConnectedMs` already present in `MQTTstuff.h:38`, `MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS` constant added) was introduced alongside the heuristic.

### 2.0.0-specific note: firmware/PIC pseudo-IDs not yet present

`OTGWfwinfoid`, `OTGWpicinfoid`, and `OTGWpicsettingsid` are not defined on this branch. `publishNonOTDiscoveryConfigs()` therefore queues only: climate (ID 0), number (ID 27), Dallas sensors (`OTGWdallasdataid`), and heap stats (`OTGWheapstatsid`).

## Decision

Identical to ADR-073 §1–§5 (including broker restart heuristic §3, now implemented).

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

Identical to ADR-073 §3. In the MQTT connect success branch, if `offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS` (5 minutes):

```cpp
if (offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS) {
    requestMQTTRepublishAll();           // republish OT data values
    clearMQTTConfigDone();               // reset discovery done-bitmap
    clearMQTTConfigPending();            // clear stale queue
    publishNonOTDiscoveryConfigs();      // queue non-OT configs immediately
    // OT ID configs re-publish JIT as messages arrive
}
```

`state.mqtt.iLastConnectedMs` is stamped on each confirmed-live `MQTTclient.loop()` tick in the `MQTT_STATE_IS_CONNECTED` handler. `iLastConnectedMs == 0` (never connected) yields `offlineMs = 0`, skipping republish on first boot.

### 4. Force path unchanged

`doAutoConfigure()` continues to call `markAllMQTTConfigPending()`. Intentional.

### 5. JIT trigger in `processOT()` unchanged

Already present and correct on this branch (lines 4087–4088 and 3546–3547).

### Trigger table (2.0.0 state)

| Trigger | Action |
|---|---|
| Boot / `startMQTT()` | `clearMQTTConfigDone()` + `clearMQTTConfigPending()` + `publishNonOTDiscoveryConfigs()`; OT IDs publish JIT |
| Top-topic changed | Same as boot |
| MQTT reconnect, offline ≤ 5 min | No action — bitmaps intact, broker retains everything |
| MQTT reconnect, offline > 5 min | `clearMQTTConfigDone()` + `clearMQTTConfigPending()` + `publishNonOTDiscoveryConfigs()` + `requestMQTTRepublishAll()` |
| HA restart (`homeassistant/status → online`) | No action — broker retains configs |
| Force (REST or Serial `F`) | `markAllMQTTConfigPending()` — all IDs queued immediately |

## Alternatives Considered

See ADR-073 — identical reasoning.

## Consequences

### Positive

Same as ADR-073: no bulk publish at boot, HA entities appear JIT, HA restart is silent.

### Negative / Risks

- Firmware/PIC info entities not queued at boot (pending arrival of `OTGWfwinfoid` etc. on this branch).
- 5-minute heuristic is approximate: a broker that restarts and recovers in under 5 minutes looks like a normal reconnect; retained configs survive in that case, so the heuristic is conservative in the right direction.

## Related Decisions

- **Ports dev ADR-073** to 2.0.0 worktree. Decisions are now fully coherent across both branches; no implementation gaps remain.
- **Supersedes 2.0.0 ADR-041 equivalent** — the JIT principle from ADR-041 is preserved; the bulk-at-boot anti-pattern is removed.
- ADR-006, ADR-040, ADR-062, ADR-067 (dev numbering equivalents) — unaffected.

## References

- dev ADR-073: `docs/adr/ADR-073-jit-ha-discovery-smart-reconnect.md` (dev worktree).
- `src/OTGW-firmware/MQTTstuff.ino:574` — `startMQTT()` change.
- `src/OTGW-firmware/MQTTstuff.ino` — `homeassistant/status` handler change (no action on HA online).
- `src/OTGW-firmware/MQTTstuff.ino` — broker restart heuristic block in connect success handler.
- `src/OTGW-firmware/OTGW-Core.ino:4087-4088` — existing JIT trigger (unchanged).

## Enforcement

```json
{
  "llm_judge": true
}
```

LLM judge guidance: In `src/OTGW-firmware/MQTTstuff.ino`, verify: (a) `startMQTT()` does NOT call `markAllMQTTConfigPending()` directly — it must call `publishNonOTDiscoveryConfigs()` instead; (b) the `homeassistant/status → online` handler does NOT call `markAllMQTTConfigPending()` or `doAutoConfigure()` — any such call is a violation; (c) `doAutoConfigure()` (the force path) MAY call `markAllMQTTConfigPending()` — that is intentional. Flag any `markAllMQTTConfigPending()` call outside of `doAutoConfigure()` and `markAllMQTTConfigPending()` itself as a violation of the JIT principle.
