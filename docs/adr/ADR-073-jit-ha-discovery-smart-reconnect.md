# ADR-073: JIT HA Discovery with Smart Reconnect (Supersedes ADR-041)

## Status

Accepted, 2026-05-08. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

ADR-041 established the JIT (Just-In-Time) discovery principle: OT MsgID discovery configs publish only when that MsgID is actually received on the OpenTherm bus, not as a bulk sweep at every boot. That principle is sound and unchanged here.

ADR-041 documented two implementation gaps:

**Gap 1 (unresolved in ADR-041):** PubSubClient v2.8 does not expose the MQTT CONNACK `sessionPresent` flag. Without it, the firmware cannot distinguish a transient reconnect (broker still has retained configs) from a broker restart (retained configs gone). ADR-041's workaround: `clearMQTTConfigDone()` on **every** successful MQTT connect, so Path B (JIT trigger in `processOT()`) re-publishes discovery as OT messages arrive. This is functionally correct but causes unnecessary re-publishing after every short network blip.

**Gap 2 (marked Resolved in ADR-041):** The `homeassistant/status → online` handler was documented as calling only `clearMQTTConfigDone()`. However, the actual implementation shipped with `markAllMQTTConfigPending()` at `MQTTstuff.ino:609`, which queues **all 256 OT IDs** for drip-publishing every time HA restarts. This contradicts the JIT goal and causes the same bulk-publish storm as a fresh boot.

Field evidence that prompted this ADR (Discord, 2026-05-08): a captured telnet log showed all 256 OT discovery configs published sequentially immediately after firmware boot, even for MsgIDs the boiler had never sent. The user explicitly verified this contradicts the JIT intent of ADR-041.

Three more specific behavioral requirements surfaced in the same session:

1. **OT MsgID configs: always JIT, never bulk.** A discovery config for an OT MsgID is published only when that MsgID is received on the bus and the done-bit is not yet set. No bulk sweep at boot, top-topic change, or HA restart.

2. **Non-OT configs: always publish at trigger points.** A named subset of "non-OT" pseudo-IDs (climate, number, Dallas sensors, heap stats, firmware info, PIC info, PIC settings) are not driven by OT bus traffic and must be published immediately at boot and at every relevant trigger (top-topic change, broker restart). These are collected in `publishNonOTDiscoveryConfigs()`.

3. **Broker restart heuristic.** In the absence of `sessionPresent`, the firmware uses a time-based heuristic: if MQTT has been disconnected for longer than `MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS` (5 minutes), assume the broker restarted and lost retained messages. On reconnect with `offlineMs > threshold`: reset both bitmaps and call `publishNonOTDiscoveryConfigs()`. Normal transient reconnects (`offlineMs <= threshold`) leave the bitmaps untouched.

## Decision

### 1. Remove bulk publish from all automatic triggers

`markAllMQTTConfigPending()` is removed from all automatic call sites. It remains available as a force utility only (see point 4).

Automatic triggers affected:
- `startMQTT()` (boot, top-topic change, MQTT restart): was `markAllMQTTConfigPending()`, now `clearMQTTConfigDone() + clearMQTTConfigPending() + publishNonOTDiscoveryConfigs()`.
- `homeassistant/status → online` handler: was `markAllMQTTConfigPending()`, now **no action**. The MQTT broker retains discovery configs across HA restarts; HA reads them via `homeassistant/#` subscription on startup.

### 2. Non-OT discovery config set (`publishNonOTDiscoveryConfigs()`)

A new function `publishNonOTDiscoveryConfigs()` queues exactly the non-OT pseudo-IDs for drip-publishing:

| Pseudo-ID constant | Entity |
|---|---|
| `0` | Climate: thermostat + DHW control |
| `27` | Number: outside temperature override |
| `OTGWdallasdataid` | Dallas temperature sensors |
| `OTGWheapstatsid` | Heap / discovery statistics |
| `OTGWfwinfoid` | Firmware info |
| `OTGWpicinfoid` | PIC info |
| `OTGWpicsettingsid` | PIC settings |

`dripDeviceInfoPending = true` is set alongside. No OT MsgID (0–255 from sensor tables) is touched.

### 3. Broker restart heuristic — piggyback on existing threshold check

In the MQTT connect success branch of `handleMQTT()` (the same block that calls `requestMQTTRepublishAll()` for data values when `offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS`):

```cpp
if (offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS) {
    requestMQTTRepublishAll();           // existing: republish OT data values
    clearMQTTConfigDone();               // new: reset discovery done-bitmap
    clearMQTTConfigPending();            // new: clear any stale queue
    publishNonOTDiscoveryConfigs();      // new: queue non-OT configs immediately
    // OT ID configs re-publish JIT as messages arrive
}
```

This resolves ADR-041's Gap 1 without requiring `sessionPresent`. Normal short reconnects (`offlineMs <= threshold`) leave discovery bitmaps intact.

### 4. Force path unchanged

`doAutoConfigure()` (REST endpoint + Serial `F` debug command) continues to call `markAllMQTTConfigPending()`, which queues all 256 OT IDs plus all non-OT pseudo-IDs. This is the explicit operator-initiated full republish and is intentionally not JIT.

### 5. JIT trigger in `processOT()` unchanged

`OTGW-Core.ino` lines 4112–4116 already implement the JIT trigger correctly:

```cpp
if (is_value_valid(OTdata, OTlookupitem) && settings.mqtt.bEnable) {
  if (!getMQTTConfigDone(OTdata.id)) {
    setMQTTConfigPending(OTdata.id);
  }
}
```

No change required here.

### Trigger table (complete)

| Trigger | Action |
|---|---|
| Boot / `startMQTT()` | `clearMQTTConfigDone()` + `clearMQTTConfigPending()` + `publishNonOTDiscoveryConfigs()`; OT IDs publish JIT |
| Top-topic changed | Same as boot (calls `startMQTT()`) |
| MQTT reconnect, offline ≤ 5 min | No action — bitmaps intact, broker retains everything |
| MQTT reconnect, offline > 5 min | `clearMQTTConfigDone()` + `clearMQTTConfigPending()` + `publishNonOTDiscoveryConfigs()` + `requestMQTTRepublishAll()` |
| HA restart (`homeassistant/status → online`) | No action — broker retains discovery configs; HA reads them via `homeassistant/#` |
| Force (REST `POST /api/v2/otgw/discovery` or Serial `F`) | `markAllMQTTConfigPending()` — all 256 OT IDs + non-OT queued immediately |

## Alternatives Considered

1. **Keep ADR-041 Gap 1 workaround (always-clear on every connect).** Rejected: causes unnecessary re-publishing after transient reconnects; defeats JIT intent for network blips.
2. **Persist done-bitmap to flash.** Considered in ADR-041, rejected then and now. Stale if `mqttha.cfg` or settings change; flash write cycles for negligible benefit.
3. **Subscribe to own discovery topics to detect broker state.** Considered in ADR-041, rejected then and now. Complex timing, non-deterministic on cooperative ESP8266.
4. **Call `clearMQTTConfigDone()` (not `markAllMQTTConfigPending()`) on HA restart.** This would re-enable JIT re-publishing as OT messages arrive. Rejected: the broker still has the retained configs; HA picks them up via its `homeassistant/#` subscription on startup without any firmware action. Adding even a bitmap-clear causes unnecessary OT bus observation delay before HA entities appear in the new instance.

## Consequences

### Positive

- **No bulk publish at boot.** HA only receives discovery configs for MsgIDs the boiler actually uses. Permanent "unknown" entities for unused MsgIDs are eliminated.
- **Normal reconnects are silent.** No discovery churn on transient network blips.
- **HA restarts are silent.** HA reads retained configs from broker without firmware action.
- **Broker restart recovery is automatic.** After 5 minutes offline, the firmware queues non-OT configs and then re-populates OT configs JIT as bus traffic resumes.
- **Force path gives a clean escape hatch.** Operators can always trigger a full republish via REST or Serial `F`.

### Negative / Risks

- **Progressive entity appearance at first boot.** OT entities appear one-by-one as messages arrive. For frequently-polled values (room temperature, setpoints) this is seconds; for rarely-transmitted diagnostic IDs it may be minutes or never.
- **5-minute heuristic is approximate.** A broker that restarts and recovers in under 5 minutes still looks like a normal reconnect to the firmware; retained configs survive anyway in that case, so the heuristic is conservative in the right direction. A broker without persistence that recovers in under 5 minutes would leave stale discovery state — mitigated by the fact that MQTT brokers with persistence (the common case) retain messages across restarts.
- **Non-OT config set is hardcoded in `publishNonOTDiscoveryConfigs()`.** Adding a new non-OT entity type requires updating the function. Mitigation: the function is small and clearly documented.

## Related Decisions

- **Supersedes ADR-041** (`Just-In-Time Home Assistant MQTT Discovery`). ADR-041's Gap 1 is resolved by the 5-minute heuristic; ADR-041's Gap 2 description was inconsistent with the shipped code — both are corrected here. After this ADR is Accepted, ADR-041's Status line will be updated to `Superseded by ADR-073, 2026-05-08.`
- ADR-006 (MQTT integration pattern), ADR-040 (source-specific topics), ADR-062 (retained discovery verification), ADR-067 (OTA discovery reconciliation) — unaffected.

## References

- Telnet log (Discord, 2026-05-08): all 256 OT configs published at boot, proving the bulk-sweep problem.
- `src/OTGW-firmware/MQTTstuff.ino:558` — `startMQTT()` change.
- `src/OTGW-firmware/MQTTstuff.ino:609` — `homeassistant/status` handler change.
- `src/OTGW-firmware/MQTTstuff.ino:783` — broker restart heuristic (existing threshold check, now also resets discovery).
- `src/OTGW-firmware/OTGW-Core.ino:4112-4116` — existing JIT trigger (unchanged).

## Enforcement

```json
{
  "llm_judge": true
}
```

LLM judge guidance: In `src/OTGW-firmware/MQTTstuff.ino`, verify: (a) `startMQTT()` does NOT call `markAllMQTTConfigPending()` directly — it must call `publishNonOTDiscoveryConfigs()` instead; (b) the `homeassistant/status → online` handler does NOT call `markAllMQTTConfigPending()` or `doAutoConfigure()` — any such call is a violation; (c) `doAutoConfigure()` (the force path) MAY call `markAllMQTTConfigPending()` — that is intentional. Flag any `markAllMQTTConfigPending()` call outside of `doAutoConfigure()` and `markAllMQTTConfigPending()` itself as a violation of the JIT principle.
