# ADR-102: HA Entity Availability Reflects the MQTT Link, Not OpenTherm-Bus Liveness (2.0.0)

## Status

Accepted. Date: 2026-05-16. Decision Maker: User: Robert van den Breemen (rvdbreemen) — approved via the cross-worktree master plan and the in-session acceptance of the dev sibling ADR-074, which encodes the identical decision.

## Context

This is the 2.0.0 (ESP32 / SAT) sibling of dev **ADR-074**. The defect and
decision are identical; only file paths and identifiers differ on this
branch.

Every Home Assistant discovery entity emitted by the firmware sets its
availability topic (`avty_t`) to the **base namespace topic**
`<toptopic>/value/<nodeid>` (`ctx.mqttPubTopic`). On the 2.0.0 line this is
done across all discovery composers in
`src/OTGW-firmware/MQTTHaDiscovery.cpp` (e.g. `kAvtyT, ctx.mqttPubTopic` at
~2150, ~2263, ~2502, ~2707 [`dhw_control`], ~2868, ~2974, ~3138). HA treats
the entity as `available` only while that topic holds `online`.

That same base topic is **also** overwritten with OpenTherm-bus liveness by
the shared helper `publishOTGWConnectedState()`
(`src/OTGW-firmware/MQTTstuff.ino:1465`):

```cpp
static void publishOTGWConnectedState()
{
  sendMQTTData(F("otgw_connected"), CCONOFF(state.otBus.bOnline));
  sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otBus.bOnline));   // <- defect
}
```

`state.otBus.bOnline = bBoilerState || bThermostatState`, each
`(now < epochXlastseen + 30)` with `now = time(nullptr)` — a 30-second
**wall-clock** window (`OTGW-Core.ino:3978`, `4018`, `4025`). The helper is
called from `processOT()` (`OTGW-Core.ino:4034`) and from
`sendMQTTstateinformation()`, so a single line removal covers both paths.

Consequence: HA entity availability is driven by OpenTherm-bus activity, not
by the MQTT connection. When OT traffic is bursty or the wall clock is
unreliable, the base topic flaps `online`/`offline`, flapping **every** HA
entity (the interactive `DHW Control` / `Thermostat` / SAT cards most
visibly). Field-confirmed on the dev line by two testers on `1.5.0+0719e08`
(Discord, 2026-05-16); the 2.0.0 line carries the same code shape.

Per-platform note: the ESP32-S3 syncs NTP faster and more reliably than the
ESP8266, so the wall-clock-instability trigger is rarer here — but the
architectural defect (a genuinely idle OT bus marking all HA entities
unavailable while the gateway is up and serving MQTT) is identical and
platform-independent. No board-specific thresholds are involved.

A dedicated OT-bus liveness sensor already exists and is unaffected:
`otgw_connected` (the `sendMQTTData(F("otgw_connected"), …)` line in the
same helper).

## Decision

HA entity availability MUST reflect only the ESP↔MQTT link. The base
namespace topic is owned exclusively by the MQTT birth/LWT mechanism:

- **Birth**: `sendMQTT(MQTTPubNamespace, "online")` retained, on connect
  (`MQTTstuff.ino:1018`) — unchanged.
- **LWT**: broker publishes `offline` retained to `MQTTPubNamespace` on
  unexpected disconnect (`MQTTclient.connect(... MQTTPubNamespace, 0, true, "offline")`,
  `MQTTstuff.ino:1000`/`1005`) — unchanged.

Remove the single OT-bus liveness write to that topic: delete the
`sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otBus.bOnline));` line
inside `publishOTGWConnectedState()` (`MQTTstuff.ino:1468`). Keep the
adjacent `sendMQTTData(F("otgw_connected"), …)`. Because the helper is
shared, this one edit fixes both the `processOT()` and
`sendMQTTstateinformation()` call paths.

No `MQTTHaDiscovery.cpp` change is required: once the liveness write is
gone, the base topic only ever carries birth `online` / LWT `offline`,
which is correct HA availability semantics for every entity. The 30-second
`time(nullptr)` window is left intact — it still correctly drives
`otgw_connected`; hardening it to `millis()` is out of scope here.

## Alternatives Considered

1. **Per-entity dedicated `avty_t` (availability list) on the climate
   entities only.** Rejected: papers over the symptom for a few entities
   while all sensors keep flapping; adds a second retained topic and more
   discovery-payload bytes; leaves base-topic semantics ambiguous.
2. **Harden the liveness window to `millis()` and keep publishing it to the
   base topic.** Rejected: reduces clock-instability flaps but does not fix
   the conflation — an idle OT bus would still mark every HA entity
   `unavailable` though the gateway is up on MQTT.
3. **`expire_after` on discovery configs instead of an availability topic.**
   Rejected: makes entities go `unavailable` when a *value* stops updating —
   wrong for slow OT IDs — and does not address the conflation.

## Consequences

### Positive

- HA `DHW Control`, `Thermostat`, SAT, and all sensor/binary-sensor/number
  entities stay `available` whenever the gateway is connected to MQTT,
  regardless of OT-bus traffic gaps or clock state.
- Availability semantics become unambiguous: the base topic means exactly
  "the OTGW's MQTT client is connected".
- Surgical change: one deleted line in a shared helper; no discovery-payload
  or schema change, no new topic. Cross-branch coherent with dev ADR-074.

### Negative / Risks

- Contract change for consumers reading the base topic as OT-bus liveness
  (the post-`/gateway`-removal behaviour carried from dev): they must
  migrate to the `otgw_connected` sensor. Documented in the changelog.
- A future contributor could reintroduce the base-topic liveness write.
  Mitigated by the Enforcement block below and code review.

## Related Decisions

- **Sibling of dev ADR-074** (`feature dev / dev` worktree) — identical
  decision; this ADR is the 2.0.0 port. Kept coherent across both lines per
  the cross-worktree workflow.
- 2.0.0 MQTT/HA discovery ADRs (ADR-097, ADR-099, ADR-101) — unaffected;
  this ADR only changes what is *not* written to the availability topic.

## References

- Discord field reports, 2026-05-16: two testers, `DHW Control` flapping,
  DHW controls dead (dev line; 2.0.0 shares the code shape).
- dev `docs/adr/ADR-074-ha-availability-reflects-mqtt-link-not-ot-bus.md`.
- `src/OTGW-firmware/MQTTstuff.ino:1465-1469` — `publishOTGWConnectedState()`
  (liveness write removed; `otgw_connected` retained).
- `src/OTGW-firmware/MQTTstuff.ino:1000`/`1005`/`1018` — LWT + birth on the
  base topic (unchanged).
- `src/OTGW-firmware/OTGW-Core.ino:4034` — `publishOTGWConnectedState()`
  caller in `processOT()`.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:2707` — `dhw_control` `avty_t`
  (representative; all composers use `ctx.mqttPubTopic`).

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "sendMQTT\\s*\\(\\s*MQTTPubNamespace\\s*,\\s*CONLINEOFFLINE",
      "path_glob": "src/OTGW-firmware/**/*.{ino,cpp,h}",
      "message": "ADR-102: do not publish OT-bus liveness to the base namespace topic — it is the HA avty_t and must reflect only the MQTT link (birth/LWT). Use the otgw_connected sensor for OT-bus liveness."
    }
  ],
  "llm_judge": false
}
```
