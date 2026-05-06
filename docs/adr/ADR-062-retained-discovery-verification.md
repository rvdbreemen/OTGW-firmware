# ADR-062 — Retained discovery verification via wildcard subscribe with RAM-tuned buffer resize

## Status

Accepted

## Context

OTGW-firmware publishes ~80 Home Assistant MQTT auto-discovery configs with `retain=true` so that HA can discover all sensor, binary_sensor, climate and number entities on a fresh install or after HA restart. The firmware tracks which configs have been published in two RAM-only bitmaps (`MQTTautoConfigMap[8]`, `MQTTautoCfgPendingMap[8]` in `OTGW-firmware.h:730-731`).

Three recovery paths exist today:

1. **OTGW MQTT reconnect**: calls `markAllMQTTConfigPending()` at `MQTTstuff.ino:434, 638`. All configs re-published via the drip.
2. **HA reboot detection** (`bHArebootDetect` setting): subscribes to `homeassistant/status`; on `offline → online` transition, calls `markAllMQTTConfigPending()` at `MQTTstuff.ino:478`.
3. **First publish after boot**: bitmaps initialise empty, so the drip publishes everything on the initial MQTT connect.

These three paths cover most real-world recovery scenarios, BUT leave two gaps:

**Gap A** — Broker-side retained-message loss while HA stays up. Causes include:
- mosquitto restart without `persistence true` in config (default)
- broker crash + recovery on a volatile file system
- manual deletion via `mosquitto_pub -r -n -t homeassistant/sensor/otgw-X/Tboiler/config`
- backup/restore of broker that misses retained state
- infrastructure migration without retained-topic replay

In all these cases HA stays connected; `homeassistant/status` does NOT transition offline→online; OTGW has no trigger to re-announce. HA keeps the entity configuration in memory but loses the state topic's retained value. Restart of HA would recover via trigger #2, but that is a heavy and user-visible action.

**Gap B** — No diagnostic surface for users troubleshooting "unknown" entities. A user asking "is the config still on the broker?" has no way to find out from the OTGW itself.

ESP8266 DRAM is ~40 KB after core libraries. Any verification approach must keep transient peak memory use minimal, especially because the verify runs during active MQTT sessions with concurrent Status-frame bursts, discovery drip (when pending > 0) and WebSocket traffic.

## Decision

Introduce an active verification mechanism that:

1. **Subscribes to the node-scoped discovery wildcard** `<haprefix>/+/<nodeId>/#` for a 15-second window.
2. **Counts retained messages received** that match our own nodeId segment; counts foreign-nodeId messages separately as "orphans".
3. **Compares received count to expected count** (`state.discovery.iPublishedTopicCount`, a new counter incremented inside each streaming discovery helper in `mqtt_configuratie.cpp` after a successful `endPublish`).
4. **Triggers `markAllMQTTConfigPending()`** only when `received < expected`, producing a full re-announce via the drip.

### Tuned parameters

| Parameter | Value | Rationale |
|---|---|---|
| Subscribe wildcard | `<haprefix>/+/<nodeId>/#` | Covers 5-segment and 6-segment discovery topics (ADR-040 source-specific variants); isolates our configs from foreign HA integrations on shared brokers |
| PubSubClient RX buffer during window | 1024 bytes (default 384) | Largest discovery config observed ~700 B, worst realistic ~900 B. 1024 covers 100% with small margin. Going to 768 would drop ~5% and cause false-positive republishes. |
| Window duration | 15 seconds | Allows slow brokers and Wi-Fi delays; early-close when `received >= expected && elapsed > 500ms` |
| Heap-abort threshold | `getFreeHeap() < 4500` | Aligned with the tightened TASK-344 thresholds; aborts window and suppresses false-missing republish |
| Heap-start threshold | `getFreeHeap() < 6000` | Must have comfortable margin above 4500 B abort |

### Coarseness — count only, not per-topic

The verification compares *counts*, not per-topic identity. A single stale retained config triggers a full re-announce of all ~80 configs. This is accepted because:

- Per-topic tracking requires a bitmap indexed by topic-hash (~32 bytes extra) OR a reverse lookup from topic → msgid (complex, fragile across source-separation modes).
- A full re-announce is cheap on the outgoing path (drip handles it, 2s between publishes).
- Broker retain-overwrite of an identical config is a no-op.
- The alternative — walking every topic to find the missing one — spends RAM and flash for marginal benefit.

### Orphan non-deletion

Retained configs that match `<haprefix>/*/config` but whose nodeId segment does NOT match ours are counted in `state.discovery.iLastOrphanCount` but NOT auto-deleted. They usually come from:

- A previous install with a different nodeId (hostname change, MAC-based nodeId from different board)
- An older firmware that published under a different naming scheme
- User running multiple OTGWs historically on the same broker

Auto-deleting would be dangerous: if the firmware incorrectly computes its own nodeId at boot (misconfiguration, corrupt settings), it could wipe a live neighbour's configs. User-visible orphan count via REST and heapdiag MQTT telemetry is sufficient; cleanup is manual with `mosquitto_pub -r -n`.

### Preconditions enforced at `startDiscoveryVerification()`

- MQTT connected (`state.mqtt.bConnected`)
- Not currently flashing (`!isFlashing()`)
- No pending drip in flight (`countPendingDiscoveryIds() == 0`) — otherwise verify counts fresh publishes as retained, producing false-OK
- Free heap ≥ 6000 bytes
- Not already active (`!isDiscoveryVerificationActive()`)

### Concurrency safety

- `handleMQTTcallback` filters verify-window messages via topic-prefix match (`<haprefix>/`); they increment `verifyReceivedCount` or `verifyOrphanCount` and return, NOT falling through to the command-handler dispatch.
- `tickDiscoveryVerification()` is polled from `handleMQTT()`. On MQTT disconnect mid-window, it fast-closes (clears flag without calling unsubscribe or buffer-restore; those are redundant on a dead client).
- Buffer-size restoration is strictly ordered: `unsubscribe → setBufferSize(384)`. Unsubscribing at the large buffer prevents truncating the UNSUBSCRIBE packet.

## Alternatives Considered

<!-- TODO: document at least 2 alternatives that were considered and rejected, with reasoning. -->

## Consequences

### Benefits

- Closes Gap A: OTGW can now detect and recover from broker-side retained-message loss without requiring HA restart.
- Closes Gap B: users have REST + telnet + MQTT telemetry to observe discovery state (`iPublishedTopicCount`, `iLastMissingCount`, `iLastOrphanCount`, `iLastVerifyEpoch`).
- Verify can be triggered on-demand (REST `POST /api/v2/discovery/verify`, telnet `V` key) for diagnosis, OR automatically once per day (TASK-350) for unattended healing.

### Costs

- Steady-state memory: +125 bytes RAM, +2.1 KB flash.
- Transient peak during verify window: +640 bytes RAM (PubSubClient RX buffer 384→1024). Released after unsubscribe.
- Each verify triggers potentially tens of KB of inbound MQTT traffic (received retained configs). Streamed through the RX buffer, not accumulated in RAM.
- False-positive republish when a retained config exceeds 1024 bytes. Accepted because no HA-standard discovery config in this codebase approaches that size; if HA ever introduces a >1KB config variant, the `iLastOrphanCount` metric will spike and ADR revision is warranted.

### Limits

- Cannot detect orphan deletion risks (intentional): foreign-nodeId configs NOT cleaned automatically.
- Cannot distinguish "retained missing" from "broker dropped during window" (e.g., transient broker issue). Both result in a re-announce; worst case is duplicated traffic.
- On large shared brokers with >200 foreign HA integrations under `homeassistant/`, the narrow `<haprefix>/+/<nodeId>/#` wildcard prevents callback flooding. Without the node-scope restriction, verify would be unviable on such brokers.
- A heap-abort during the verify window is indistinguishable in telemetry from a clean pass (`iLastMissingCount = 0`). If `iLastVerifyEpoch` advances but `iRepublishTriggeredCount` never does, check the debug log for `[verify] heap-abort` to distinguish. Follow-up: TASK-361 introduces an explicit outcome enum.
- At boot, the `dayChanged()` helper's `lastX = -1` sentinel fires true on the first post-NTP-sync minute. With `MQTTdiscoveryAutoVerify = true`, a verify pass runs within one minute of reaching NTP sync, not at the wall-clock day boundary. Intentional: covers the case where HA was restarted while OTGW was offline.

### Binding rule enforced by this ADR

- Every `stream*Discovery` helper in `mqtt_configuratie.cpp` (currently `streamSensorDiscovery`, `streamBinarySensorDiscovery`, `streamClimateDiscovery`, `streamNumberDiscovery`, `streamDallasSensorDiscovery`) MUST call `incPublishedTopicCount()` after a successful `endPublish`. Missing calls cause `iPublishedTopicCount` to undercount, resulting in persistent false-positive republish triggers.
- `clearMQTTConfigDone()` in `MQTTstuff.ino` MUST reset `state.discovery.iPublishedTopicCount = 0` so the counter stays consistent with the `MQTTautoConfigMap` bitmap.

These binding rules need CI-gate entries in `evaluate.py`:

- `check_discovery_counter_instrumented`: greps `mqtt_configuratie.cpp` for every `stream*Discovery` helper and ensures each has a matching `incPublishedTopicCount()` call within the function body after the final `endPublish()`.
- `check_publishedtopic_counter_reset`: greps `clearMQTTConfigDone` body for `state.discovery.iPublishedTopicCount = 0` (or equivalent assignment).

These gates land in TASK-349.

## Related Decisions

- ADR-004 — no String in hot paths (all new code uses `char[]` + `snprintf_P`)
- ADR-040 — source-specific topics explain why wildcard must be `/#`, not `/+/config`
- ADR-044 — global state access from secondary TUs; explains why `incPublishedTopicCount` is a shim rather than direct state access
- ADR-050 — centralized API route dispatch; REST `discovery` route added to `kV2Routes`
- ADR-051 — state/settings split with Hungarian prefixes; new `DiscoverySection` follows this
- TASK-348 — pending-bit limbo fix (prerequisite; otherwise verify-triggered republish itself leaks msgids)
- TASK-349 — this ADR's implementation
- TASK-351 — time-boundary dispatcher unification (prerequisite for TASK-350 auto-verify trigger)
- TASK-350 — daily automatic verify triggered from unified dispatcher

## References

<!-- TODO: populate from inline citations or external sources cited in the body. -->

## Enforcement

```json
{
  "llm_judge": true
}
```
