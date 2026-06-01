# ADR-112: Pure JIT MQTT Discovery (2.0.0 sibling of dev ADR-073)

## Status

Accepted, 2026-05-27

Classification: structural (per ADR-080: no CI gate; reviewed at PR).

## Context

Until TASK-578 the 2.0.0 branch carried two boot/reconnect paths that called
`markAllMQTTConfigPending()`:

1. `startMQTT()` — every cold boot / top-topic change.
2. The `homeassistant/status` "broker offline > 5 min" path inside `handleMQTT()`.

`markAllMQTTConfigPending()` iterates the full 256-bit OT ID space and queues
every ID that appears in the PROGMEM discovery tables, plus the non-OT pseudo
entries (climate, Dallas, heap stats, firmware/PIC info, OTDirect diagnostics,
SAT zones). On a freshly booted gateway this produced a discovery storm of
~80–120 retained `homeassistant/.../config` publishes that the broker then
re-fanned to Home Assistant — a heap and TCP burst at the worst possible moment
(WiFi just up, OT bus not yet read, heap fragmented from boot).

The dev branch already resolved this in TASK-577 via ADR-073's JIT policy: at
boot we publish only the small set of *non-OT* discovery entities; OT ID
configs are published *just-in-time* the first time each MsgID is observed on
the bus. This makes the boot-storm proportional to the actual installed
hardware: a thermostat-only setup never publishes DHW-only configs, a
master-mode gateway never publishes slave-only configs, etc.

The 2.0.0 branch needs the same policy. Topic shape, retained semantics and
the underlying drip publisher are unchanged from dev — this is purely about
*who decides* an OT ID's config needs to be on the broker.

## Decision

On the 2.0.0 branch (`feature-dev-2.0.0-otgw32-esp32-sat-support`):

1. Replace `markAllMQTTConfigPending()` in `startMQTT()` with the triplet
   `clearMQTTConfigDone() + clearMQTTConfigPending() + publishNonOTDiscoveryConfigs()`.
2. Replace `markAllMQTTConfigPending()` in the broker-restart path
   (`offlineMs > MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS`) with the same triplet.
3. Keep `markAllMQTTConfigPending()` available for the explicit user-triggered
   "republish HA discovery" REST endpoint and for `mqtt_discovery_verify.cpp`'s
   missing-config recovery path — those callers genuinely want everything
   re-queued.
4. `publishNonOTDiscoveryConfigs()` queues only:
   - ID 0   — climate (thermostat + DHW control)
   - ID 27  — number (outside temperature override)
   - `OTGWdallasdataid`   — Dallas temperature sensors
   - `OTGWheapstatsid`    — heap / discovery statistics
   - `OTGWfwinfoid`       — firmware info
   - `OTGWpicinfoid`      — PIC info
   - `OTGWpicsettingsid`  — PIC settings
   - `OTGWpiccontrolsid`  — PIC controls (resetgateway button, GPIO/LED selects)

   It also sets `dripDeviceInfoPending = true` so the first drip entity carries
   the full HA device block (required by ADR-077's streaming architecture).
5. OT ID discovery is emitted by the existing JIT publisher inside the OT
   processing pipeline the first time each MsgID is observed (no change).

## Alternatives Considered

1. **Keep `markAllMQTTConfigPending()` on every boot/reconnect** (status quo).
   Rejected: it queues the full 256-bit OT ID space + all pseudo entities,
   producing an ~80–120 retained-config discovery storm at the worst moment
   (WiFi just up, OT bus unread, heap fragmented from boot).
2. **Throttle/delay the full boot publish** (spread the same ~120 configs over
   time). Rejected: still advertises entities for hardware that may never appear
   (DHW configs on a thermostat-only setup), so the broker retained set stays
   based on what *could* appear, not what *did* — JIT fixes the set, not just its
   timing.
3. **Pure JIT, mirroring dev's ADR-073/TASK-577** (chosen): publish only the
   small non-OT set at boot; emit each OT ID's config just-in-time on first
   observation of that MsgID. Boot-storm drops to ~8 publishes; the retained set
   converges to the actually-installed hardware; reconnect matches dev
   byte-for-byte. The explicit user "republish all" REST path retains
   `markAllMQTTConfigPending()` as the escape hatch.

## Consequences

Positive:

- Boot-storm reduced from ~80–120 retained discovery publishes to ~8 at boot,
  with the remainder paced one-per-bus-message as the OT traffic establishes
  itself. Heap pressure during cold boot is substantially lower.
- Broker retained-config set converges to the *actual* hardware: a gateway
  that never sees DHW frames never advertises DHW entities to HA. Previously
  the discovery set was based on what *could* appear, not what *did*.
- 2.0.0 reconnect-after-broker-restart now matches dev byte-for-byte at the
  policy level — no behavioural divergence between branches.

Negative / trade-offs:

- Hardware that briefly emits a MsgID at boot and never again will keep that
  retained config on the broker forever (until manual cleanup). ADR-093
  governs orphan cleanup; this decision does not change that contract.
- A user looking at HA *immediately* after gateway boot now sees entities
  populate over the first ~30 seconds of OT traffic instead of all at once.
  The progressive reveal is a UX trade we accept in exchange for heap headroom.
- The explicit user-triggered "republish HA discovery" REST endpoint
  (`restAPI.ino:1589`) still uses `markAllMQTTConfigPending()` and remains the
  documented escape hatch when a user *wants* the full set re-pushed.

## Related Decisions

- ADR-073 (dev origin — the JIT discovery boot policy this ADR ports to 2.0.0)
- ADR-077 (streaming MQTT HA discovery architecture — the drip publisher + the
  device-block-on-first-entity requirement honoured by `dripDeviceInfoPending`)
- ADR-093 (HA discovery retained-config orphan cleanup — governs the
  briefly-seen-MsgID retained-config trade-off noted under Consequences)
- ADR-080 (binding ADRs need a CI gate; this one is structural, reviewed at PR)

## References

- TASK-578 (this 2.0.0 implementation); TASK-577 (the dev-side implementation
  under ADR-073)
- `publishNonOTDiscoveryConfigs()` / `markAllMQTTConfigPending()` in
  `MQTTstuff.ino`; the user-republish escape hatch at `restAPI.ino:1589`;
  missing-config recovery in `mqtt_discovery_verify.cpp`

## Verification

- Build green: `python build.py --firmware` exit 0 on the 2.0.0 esp32 target.
- Evaluator green: `python evaluate.py --quick` reports no new failures.
- Manual: boot a gateway with an empty MQTT broker; observe that only the
  non-OT discovery configs land on `homeassistant/.../config` before any OT
  frame is processed. OT ID configs appear paced as MsgIDs arrive on the bus.
