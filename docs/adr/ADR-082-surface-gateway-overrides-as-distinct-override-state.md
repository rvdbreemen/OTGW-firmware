# ADR-082: Surface gateway overrides as distinct override state

## Status

Accepted, 2026-06-01.

## Context

### Problem statement

When the OTGW gateway substitutes OpenTherm traffic for a message ID (a user-configured override such as `OT=` outside temperature, `TT=`/`TC=` room setpoint, `SW=` DHW setpoint, `MM=` max modulation), the firmware's boiler-side-worldview gate `is_value_valid_for_master_topic()` (`src/OTGW-firmware/OTGW-Core.ino`, ADR-069 + ADR-075) deliberately excludes the gateway-injected value from the canonical state: an answer-override `A` frame (`OTGW_ANSWER_THERMOSTAT && bAnswerOverride`) and a substituted thermostat `T` frame (`OTGW_THERMOSTAT && bGatewaySubstituted`) do not update `OTcurrentSystemState` and are not published to the canonical MQTT topic. That is correct for canonical (canonical must reflect boiler-side reality), but it makes the user-injected override **invisible**.

Concrete field report (Discord #nederlandse-ondersteuning, Richard_HA, 2026-06-01, fw 1.6.1, Remeha boiler without outside sensor): the user injects `OT=20.5` via Node-RED → REST → PIC. A telnet capture confirms the command reaches the PIC and the gateway answers the thermostat with `A4..1480` (= 20.5 °C), while the boiler answers `Data-Invalid` on ID 27 every poll. Because the canonical value is fed only by the (Data-Invalid) boiler frame, the Web UI "Outside Temperature" shows `0`. Before v1.5.0 the value was visible because thermostat-side and boiler-side were conflated; the ADR-069/075 worldview split made canonical correct but hid the override. Users legitimately expect that a value they actively inject is visible somewhere.

### Background

The override values are already computed inside the `print_f88()` decode path; they are simply not stored when the master-topic gate rejects the frame. The detection flags (`bGatewaySubstituted`, `bAnswerOverride`) are set in `processOTGW()` (`OTGW-Core.ino` ~4083-4128) via the existing 500 ms `(B,A)`/`(T,R)` pairing. An `Outside Temperature Override` HA number entity already exists (`mqtt_configuratie.cpp` `streamNumberDiscovery`), but its `stat_t` points at the canonical `/Toutside` topic, so it too displays `0` (tracked as TASK-804).

### Constraints

- **ESP8266 RAM (~40 KB).** New state must be small and use fixed `char[]`/structs, not `String` (ADR-004).
- **Canonical contract must not change.** ADR-069 and ADR-075 are Accepted and authoritative for canonical/worldview routing; this decision must be purely additive and must not alter what canonical publishes.
- **Cooperative scheduling / re-entrancy.** `print_*` runs in the OT decode path; MQTT publish yields via `feedWatchDog`. New capture logic must not publish inline across a yield with shared buffers (ADR-style static-buffer discipline).
- **Immutable accepted ADRs.** ADR-069/075 cannot be edited; this ADR refines them without superseding.

### Stakeholders

End users (visibility of their own overrides in Web UI / Home Assistant), Home Assistant integrators (MQTT entities), maintainers (added surface to maintain).

## Decision

**Decision maker: User: Robert van den Breemen** (chose: cover ALL gateway override message types, not just Toutside; expose via Web UI + REST + MQTT/HA; dev release becomes 1.7.0).

Introduce an additive, per-message-ID **override-state store** that records the gateway-injected value the worldview gate drops, and expose it on all three surfaces. Canonical behaviour is unchanged.

1. **Capture by explicit flags, not `!validForMaster`.** In `print_f88()` record an override only when `(OT.rsptype==OTGW_ANSWER_THERMOSTAT && OT.bAnswerOverride) || (OT.rsptype==OTGW_THERMOSTAT && OT.bGatewaySubstituted)`. `validForMaster` is false for non-override reasons (e.g. a WRITE-ACK on an `OT_WRITE` id) and must not be used as the trigger. All nine numeric override-capable IDs are `ot_f88` (TSet 1, TsetCH2 8, TrOverride 9, MaxRelModLevelSetting 14, TrSet 16, Toutside 27, TrOverride2 39, TdhwSet 56, MaxTSet 57), so `print_f88()` is the only capture site. IDs 99/100 (operating-mode/flag status, not injected numeric values) are out of scope for this ADR.

2. **Store a `kind` discriminator.** The captured value means different things per case: for answer-override `A` frames it is the gateway-forced answer shown to the thermostat (= the user-injected value); for substituted `T` frames it is the thermostat's original value that was replaced (the forced `R` value reaches canonical separately). The store records `kind` so each surface labels it correctly.

3. **Active/stale via the entry's own timestamp.** Each entry carries its own `lastSeen` (updated only by the capture hook); `active = (millis() - lastSeen) < OVERRIDE_ACTIVE_TIMEOUT` (~10 min). The store must **not** be cleared by "the next valid frame", because during an active answer-override the real boiler `B` frame arrives and passes the gate every poll, which would wrongly clear the override each cycle.

4. **Pure-RAM capture; publish from the periodic path.** The hook only writes to the store (no yield, no shared buffer). MQTT publication and REST/Web UI reads happen on the existing periodic / request paths that already own their buffer discipline.

5. **Additive surfaces.**
   - REST: new `GET /api/v2/otgw/overrides` streamed via the existing JSON helpers, wired as a new branch in `handleOtgw()`. No existing route or response changes.
   - Web UI: new "Active Overrides" table (Statistics tab) polled by a new `refreshOtOverrides()`, rendered with `textContent` and element-existence checks.
   - MQTT/HA: retained `<base>/<label>/override` published from the periodic path, with JIT HA discovery; the existing `Toutside_override` number-entity `stat_t` is retargeted to the override state topic so it reflects the injected value (closes TASK-804).

6. **Storage shape.** `OTOverrideEntry_t { uint8_t id; uint8_t kind; float value; uint32_t lastSeen; }` × `OVERRIDE_STORE_MAX = 11` ≈ 132 bytes static, declared `extern` in the core header and defined once, with `recordOTOverride()` / `isOTOverrideActive()` helpers (linear find-or-allocate scan).

## Alternatives Considered

### Alternative 1: Stop excluding overrides from canonical (revert to pre-1.5.0 conflation)
Let the override value flow into canonical so the existing sensor shows it.
**Why not chosen:** Breaks the ADR-069/075 boiler-side-worldview contract; canonical would no longer represent what the boiler actually sees, reintroducing the data-loss/ambiguity bug those ADRs fixed. The injected value and the boiler-side reality are genuinely different quantities and must stay separate.

### Alternative 2: Web-UI/REST only, no MQTT/HA
Surface overrides only locally; do not add MQTT topics or HA entities.
**Why not chosen:** The reported pain point includes Home Assistant (the existing `Toutside_override` number entity shows `0`); leaving MQTT out would not close TASK-804 and would split the feature across releases inconsistently. Maintainer chose full exposure.

### Alternative 3: Track only the command the ESP sent (`OT=`, `TT=`, ...) instead of observing the bus
Remember the last override command issued via REST/MQTT.
**Why not chosen:** Misses overrides set directly on the PIC (not via the ESP) and overrides already active at boot; the bus observation via `bAnswerOverride`/`bGatewaySubstituted` is the single source of truth that captures all paths.

## Consequences

### Positive
- Users see their active overrides in Web UI, REST and Home Assistant, even when the boiler ignores them (`Data-Invalid`), matching the legitimate user expectation.
- Canonical behaviour is byte-for-byte unchanged; existing endpoints, topics and the `OTcurrentSystemState` contract are untouched (purely additive).
- Closes TASK-804 by giving the `Toutside_override` number entity a state source that reflects the injected value.
- Refines ADR-069/075 without editing them; the worldview split stays authoritative for canonical.

### Negative / risks
- Added surface to maintain (one REST endpoint, one Web UI section, one MQTT topic family + discovery) and ~132 bytes static RAM.
- **Risk:** capturing on `!validForMaster` instead of the explicit flags would record non-override frames. **Mitigation:** capture is gated on the two explicit rsptype/flag combinations only.
- **Risk:** clearing the store on a valid frame would erase an active override every poll (boiler `B` passes the gate). **Mitigation:** active/stale uses the entry's own `lastSeen` + timeout, never a "valid frame clears it" rule.
- **Risk:** publishing inline from the decode hook across a `feedWatchDog` yield could corrupt shared static buffers. **Mitigation:** the hook is a pure RAM write; publishing stays on the periodic path.
- IDs 99/100 (operating-mode / override-function flags) are not covered; documented as out of scope, can be a follow-up.

## Enforcement

No mechanical `forbid_pattern`/`require_pattern` cleanly expresses this decision (it spans a new store, a decode-path hook, and three additive surfaces). This ADR is **manual-review only**. Reviewers must confirm: (a) the canonical path is unchanged — no edits to what `is_value_valid_for_master_topic()` admits to canonical, to `OTcurrentSystemState` semantics, or to existing REST/MQTT canonical output; (b) capture is gated on the explicit `bAnswerOverride`/`bGatewaySubstituted` combinations, not `!validForMaster`; (c) the override store is not cleared by subsequent valid frames; (d) the capture hook performs no MQTT publish / yields.

## Related Decisions

- Refines [ADR-069](ADR-069-mqtt-source-topic-worldview-semantics.md) and [ADR-075](ADR-075-mqtt-source-topic-proxy-answer-routing.md) (boiler-side-worldview canonical gating). This is an additive amendment that surfaces the *dropped* override value separately; it does not change canonical routing, so those ADRs remain Accepted and authoritative and their Status lines are intentionally left unchanged.
- Implementation: TASK-805 (dev). 2.0.0 sibling: TASK-806 + ADR-118 (2.0.0 numbering). Absorbs TASK-804.
- Code: `src/OTGW-firmware/OTGW-Core.h` (store + extern), `OTGW-Core.ino` (`print_f88` capture hook, `recordOTOverride`/`isOTOverrideActive`, periodic publish), `restAPI.ino` (`handleOtgw` new `overrides` branch), `mqtt_configuratie.cpp` (`streamOverrideSensorDiscovery`, `Toutside_override` `stat_t` retarget), `MQTTstuff.ino` (`publishDiscoveryFor` dispatch), `data/index.html` + `data/index.js` (Active Overrides view).

## References

- Field report + telnet capture (`Telnet_LOG_OTGW.txt`): Discord #nederlandse-ondersteuning, Richard_HA, 2026-06-01. Confirms `MQTT .../outside=19.7` → `OT=19.7` to PIC, gateway answers thermostat `A4..1480` (20.5 °C), boiler `Data-Invalid` on ID 27, canonical `/Toutside` = 0.
- Override detection: `src/OTGW-firmware/OTGW-Core.ino` `processOTGW()` ~4083-4128 (`bGatewaySubstituted`, `bAnswerOverride`, 500 ms pairing).
- Canonical gate: `is_value_valid_for_master_topic()` ~1162; capture site `print_f88()` ~1863-1885.
- Existing override entity: `mqtt_configuratie.cpp` `streamNumberDiscovery()` (`Toutside_override`, `cmd_t=.../outside`, `stat_t=.../Toutside`).
- Master plan: `~/.claude/plans/mooi-ja-dit-is-ethereal-tulip.md`.
