# ADR-074: HA Entity Availability Reflects the MQTT Link, Not OpenTherm-Bus Liveness

## Status

Accepted, 2026-05-16. Decision Maker: User: Robert van den Breemen (rvdbreemen).

## Context

Every Home Assistant discovery entity emitted by the firmware sets its
availability topic (`avty_t`) to the **base namespace topic**
`<toptopic>/value/<nodeid>` (`ctx.mqttPubTopic`). This is true for all five
discovery composers in `src/OTGW-firmware/mqtt_configuratie.cpp` (sensor
~1992, binary sensor ~2094, Dallas ~2313, climate Thermostat + DHW Control
~2518, number ~2676). HA treats the entity as `available` only while that
topic holds `online` and `unavailable` when it holds `offline`.

That same base topic is **also** overwritten with OT-bus liveness:

- `src/OTGW-firmware/OTGW-Core.ino:4029` — in `processOT()`, on every
  `state.otgw.bOnline` transition: `sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline))`.
- `src/OTGW-firmware/MQTTstuff.ino:1113` — in `sendMQTTstateinformation()`:
  the same publish.

`state.otgw.bOnline = bBoilerState || bThermostatState`, where each side is
`(now < epochXlastseen + 30)` and `now = time(nullptr)`
(`OTGW-Core.ino:3977`, `4011`, `4018`, `4025`) — a 30-second window keyed off
**wall-clock** time, not `millis()`.

Consequence: HA entity availability is driven by OpenTherm-bus activity, not
by whether the gateway is connected to MQTT. When OT traffic is bursty, or
the wall clock used by the 30 s window is unreliable (no/late NTP, or an
NTP that resyncs and jumps), the base topic flaps `online`/`offline`, so
**every** HA entity flaps available/unavailable. The interactive climate
cards (`DHW Control`, `Thermostat`) are the most visible: while
`unavailable` their state hides and their command widgets are disabled.

This regressed at v1.5.0. Before 1.5.0 the online/offline status lived on a
dedicated `/gateway` sub-topic; TASK-538 ("`/gateway` sub-topic removed;
canonical base topic replaces it", CHANGELOG 1.5.0) moved it onto the
canonical base topic — which is exactly the topic the discovery composers
use as `avty_t`.

Field evidence (Discord, 2026-05-16): two independent testers on
`1.5.0+0719e08` reported the `DHW Control` entity flapping — one measured
"~20 s unavailable, ~2 s available", another "DHW enabled goes on/off every
2 s" — with no DHW settings shown and the DHW temperature / on-off controls
dead. Code inspection confirmed `dhw_enable` is single-source and gated, so
it cannot toggle on its own at that cadence; the cause is the `avty_t`
flap, not the state value.

A dedicated OT-bus liveness sensor already exists and is unaffected by this
problem: `otgw-pic/otgw_connected` (published by the `sendMQTTDataPic(F("otgw_connected"), …)`
companion lines at `OTGW-Core.ino:4028` and `MQTTstuff.ino:1112`).

## Decision

HA entity availability MUST reflect only the ESP↔MQTT link. The base
namespace topic carries availability and is owned exclusively by the MQTT
birth/LWT mechanism:

- **Birth**: `sendMQTT(MQTTPubNamespace, "online")` retained, on MQTT
  connect (`MQTTstuff.ino:774`) — unchanged.
- **LWT**: broker publishes `offline` retained to `MQTTPubNamespace` on
  unexpected disconnect (`MQTTclient.connect(... MQTTPubNamespace, 0, true, "offline")`,
  `MQTTstuff.ino:756`/`761`) — unchanged.

Remove the two OT-bus liveness writes to that topic:

- `src/OTGW-firmware/OTGW-Core.ino:4029` — delete the
  `sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline));` line.
  Keep the adjacent `sendMQTTDataPic(F("otgw_connected"), CCONOFF(state.otgw.bOnline));`.
- `src/OTGW-firmware/MQTTstuff.ino:1113` — delete the identical line in
  `sendMQTTstateinformation()`. Keep the adjacent `otgw_connected` publish.

OpenTherm-bus liveness remains fully observable via the `otgw_connected`
sensor, which is unchanged. The 30-second `time(nullptr)` liveness window is
left intact — it still correctly drives `otgw_connected`; hardening that
window to `millis()` is explicitly out of scope for this ADR and tracked
separately if pursued.

No discovery composer changes are required: once the liveness write is
gone, the base topic only ever carries birth `online` / LWT `offline`, which
is exactly correct HA availability semantics for every entity.

## Alternatives Considered

1. **Give the climate entities a separate `avty_t` (e.g. an
   `availability`-list with a dedicated MQTT-link topic) and leave the base
   topic carrying OT-liveness.** Rejected: it only papers over the symptom
   for two entities while the other ~300 sensor/binary-sensor/number
   entities keep flapping; it adds a second retained availability topic and
   more discovery payload bytes on a RAM-constrained ESP8266; and it leaves
   a topic whose semantics ("online" sometimes means MQTT-up, sometimes
   means OT-bus-active) ambiguous.
2. **Harden the liveness window to `millis()` and keep publishing it to the
   base topic.** Rejected: it reduces flap frequency from clock instability
   but does not fix the architectural conflation — a genuinely idle OT bus
   (boiler standby, slow-polling thermostat) would still mark every HA
   entity `unavailable`, which is incorrect: the gateway is up and serving
   MQTT.
3. **Add `expire_after` to discovery configs instead of an availability
   topic.** Rejected: `expire_after` makes entities go `unavailable` when a
   *value* stops updating — the same wrong behaviour for slow OT IDs — and
   does not address the base-topic conflation.
4. **Restore a dedicated `/gateway` sub-topic for OT-liveness (revert
   TASK-538's topic move).** Rejected: `otgw_connected` already provides the
   dedicated OT-liveness signal; reintroducing `/gateway` would duplicate it
   and re-expand the topic surface TASK-538 deliberately reduced.

## Consequences

### Positive

- HA `DHW Control`, `Thermostat`, and all sensor/binary-sensor/number
  entities stay `available` whenever the gateway is connected to MQTT,
  regardless of OT-bus traffic gaps or clock state. Fixes the field-reported
  flap for all entities, not just DHW.
- Availability semantics become unambiguous: the base topic means exactly
  "the OTGW's MQTT client is connected".
- Surgical change: two deleted lines, no discovery-payload or schema change,
  no new topic.

### Negative / Risks

- **Contract change for consumers reading the base topic as OT-liveness.**
  Any user automation or dashboard that subscribed to
  `<toptopic>/value/<nodeid>` expecting `online`/`offline` to track the
  OpenTherm bus (the post-1.5.0/TASK-538 behaviour) must migrate to the
  `otgw_connected` sensor topic. This is documented in the changelog and
  release notes. The base topic still publishes `online`/`offline`, but now
  strictly for the MQTT link.
- No behavioural change for the common case (HA discovery users), who get a
  strictly better experience.

## Related Decisions

- TASK-538 (v1.5.0: `/gateway` sub-topic removed, canonical base topic
  replaces it) — introduced the regression vector this ADR corrects.
- ADR-006 (MQTT integration pattern), ADR-062 (retained discovery
  verification), ADR-073 (JIT HA discovery) — unaffected; this ADR only
  changes what is *not* written to the availability topic.
- 2.0.0 sibling: ADR-102 in the
  `feature-dev-2.0.0-otgw32-esp32-sat-support` worktree applies the same
  decision to the ESP32/SAT line (`MQTTHaDiscovery.cpp`,
  `publishOTGWConnectedState()`), cross-referencing this ADR.

## References

- Discord field reports, 2026-05-16: two testers on `1.5.0+0719e08`,
  `DHW Control` flapping, DHW controls dead.
- CHANGELOG.md `[1.5.0]` → Changed → "`/gateway` sub-topic removed;
  canonical base topic replaces it (TASK-538)".
- `src/OTGW-firmware/mqtt_configuratie.cpp:2518` — climate `avty_t = ctx.mqttPubTopic`
  (and 1992, 2094, 2313, 2676 — all composers).
- `src/OTGW-firmware/OTGW-Core.ino:4028-4029` — `otgw_connected` (kept) +
  base-topic liveness write (removed).
- `src/OTGW-firmware/MQTTstuff.ino:1112-1113` — same pair in
  `sendMQTTstateinformation()`.
- `src/OTGW-firmware/MQTTstuff.ino:756`/`761`/`774` — LWT + birth on the
  base topic (unchanged).
- `src/OTGW-firmware/OTGW-Core.ino:3977`,`4011`,`4018`,`4025` — the
  `time(nullptr)` 30 s liveness window (unchanged; drives `otgw_connected`).

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "sendMQTT\\s*\\(\\s*MQTTPubNamespace\\s*,\\s*CONLINEOFFLINE",
      "path_glob": "src/OTGW-firmware/**/*.{ino,cpp,h}",
      "message": "ADR-074: do not publish OT-bus liveness to the base namespace topic — it is the HA avty_t and must reflect only the MQTT link (birth/LWT). Use the otgw_connected sensor for OT-bus liveness."
    }
  ],
  "llm_judge": false
}
```
