# ADR-085: Unified Off/Heat/Cool Home Assistant Climate Entity Derived from OpenTherm Status Bits

## Status

Accepted. Date: 2026-06-27.

**Decision Maker:** User: Robert van den Breemen (maintainer). The design was implemented on this `otgw-1.x.x` maintenance line, shipped as `1.7.1-beta.2` (commit `7b2d3cdd`, GitHub issue #665), and validated against real OpenTherm captures before acceptance. The 2.0.0 `dev` line carries the sibling decision in its own numbering as ADR-156 (port tracked under TASK-939).

## Status History

status_history:
  - date: 2026-06-27
    status: Accepted
    changed_by: Agent
    reason: Records the heat/cool/off HA climate representation shipped on the 1.x line (GH #665, 1.7.1-beta.2, commit 7b2d3cdd); validated against four real captures
    changed_via: adr-kit

## Context

The OpenTherm (OT) gateway exposes a single Home Assistant (HA) MQTT `climate` entity ("Thermostat") for the room. Before this decision that entity was heating-only. In `streamClimateDiscovery` (`mqtt_configuratie.cpp`, `climateIdx == 0`):

- `modes` was `["off","heat"]`,
- `mode_stat_t` bound a connection/CH-derived topic with an `ON -> heat / else off` template,
- `action_topic` bound a CH-enable-derived topic with an `ON -> heating / else idle` template,
- the target setpoint is `TrSet` (MsgID 16, the room setpoint) on `temp_stat_t`,
- `max_temp` was `28`.

A user on GitHub issue #665 runs a Honeywell Round Modulation Heat/Cool thermostat on a heatpump that both heats and cools, with an OTGW in between. The gateway relays cooling correctly at the OpenTherm level: the master sets `cooling_enable` (MsgID 0 high byte, bit 2), the boiler/heatpump answers with the cooling status bit (MsgID 0 low byte, bit 4), and demand is carried as a percentage on MsgID 7 (CoolingControl). The defect was purely in the HA representation: with `modes` fixed at `["off","heat"]` there was no `cool` mode, so while the system cooled, HA still showed a heating thermostat carrying the cooling setpoint, with no control to switch back. Cooling was invisible and the entity was misleading.

The firmware already decodes every bit this needs. The OT status bytes are tracked as `OTcurrentSystemState.MasterStatus` (MsgID 0 HB) and `OTcurrentSystemState.SlaveStatus` (MsgID 0 LB) in `OTGW-Core.ino`, and thermostat liveness is tracked as `state.otgw.bThermostatState`. The gap was that none of these reached the climate discovery payload; the entity was wired to connection state and CH-enable only.

## Decision

Surface a single unified `off`/`heat`/`cool` climate entity, driven by two new firmware-computed MQTT topics, with mode switching kept reflective (the firmware mirrors the thermostat, it does not command it).

### Two new computed topics

The firmware computes and publishes two values from the OT status bits, in the `OTGW-Core.ino` publish path alongside the existing status-bit fan-out:

1. **`<pub>/hvac_mode`** in `{off, heat, cool}` (`publishHvacMode`, `OTGW-Core.ino:1654`):
   - `cool` when the master `cooling_enable` bit is set (`MasterStatus & 0x04`, MsgID 0 HB bit 2),
   - else `heat` when the thermostat is connected (`state.otgw.bThermostatState`),
   - else `off` (only when the thermostat is disconnected).
2. **`<pub>/hvac_action`** in `{off, idle, heating, cooling}` (`publishHvacAction`, `OTGW-Core.ino:1671`), evaluated **cooling-before-heating** so the precedence is unambiguous if both slave bits are ever set together:
   - `off` when the thermostat is disconnected,
   - else `cooling` when the slave cooling bit is set (`SlaveStatus & 0x10`, MsgID 0 LB bit 4),
   - else `heating` when the slave central-heating bit is set (`SlaveStatus & 0x02`, MsgID 0 LB bit 1),
   - else `idle`.

Both are published on change and on force (`publishHvacMode(forcePublish)` is called from `publishMasterStatusState`, `OTGW-Core.ino:1725`; `publishHvacAction(forcePublish)` from `publishSlaveStatusState`, `:1770`; and `publishHvacMode` is re-evaluated on thermostat connect/disconnect at `:4137`, so the entity reads `off` the moment the thermostat goes away).

### Discovery payload changes

In `streamClimateDiscovery` (`climateIdx == 0`, `mqtt_configuratie.cpp:2580`):

- `modes` becomes `["off","heat","cool"]` (static, never a per-state single-element list) (`:2652`),
- `mode_stat_t` becomes `<pub>/hvac_mode` with a pass-through `mode_stat_tpl` of `{{ value }}` (the firmware already emits the literal mode word) (`:2659-2667`),
- `action_topic` becomes `<pub>/hvac_action` with a pass-through `action_template` of `{{ value }}` (`:2602-2608`),
- `max_temp` widens from `28` to `30` (cooling setpoints sit above the heating comfort band) (`:2709`),
- the single setpoint stays `TrSet` (MsgID 16) on `temp_stat_t` (`:2689`); `temp_cmd_t` stays `<sub>/command` with `TT={{ value }}` (`:2696-2701`). `TSet` (MsgID 1, the boiler water-control setpoint) is **not** surfaced as the user target.

### Mode is reflective: no `mode_command_topic`

The entity has no `mode_command_topic`. Heat/cool selection is owned by the thermostat (the OpenTherm master), not by the in-line gateway: the thermostat sets the `cooling_enable` bit (MsgID 0 HB bit 2) and re-asserts the MsgID 0 status on the bus roughly every 7 seconds. The OpenTherm MsgID 3 Slave Configuration flag (HB bit 7, "Heat/cool mode control") designates whether the master or the slave performs the heat/cool switching (clear: switching can be done by master; set: switching is done by slave); in neither case is the gateway the controller. The OTGW mirrors the master's mode into HA; it does not originate it. An HA-side mode command would fight the master and be overwritten on the next status cycle, so offering the control would be a lie. HA can still command the **setpoint** (`TrSet` via `TT=`); only the mode is read-only.

Heating-only and gas-boiler users are unaffected: their `cooling_enable`/cooling bits are never set, so `hvac_mode` never becomes `cool` and `hvac_action` never becomes `cooling`.

## Alternatives Considered

### Alternative A: Dual-setpoint `HVACMode.HEAT_COOL` (auto, with `target_temp_high` + `target_temp_low`)

Model the system as HA's auto/heat_cool mode with a high and low bound. Rejected as **infeasible against the OpenTherm protocol**, not merely undesirable. OpenTherm exposes exactly one mode-agnostic room setpoint (`TrSet`, MsgID 16); cooling is commanded as a percentage demand (MsgID 7 CoolingControl), never as a second temperature. The two bounds never coexist on the wire: in the real #665 cooling capture the heating setpoint (around 19 C) never appears while cooling. Faking the two bounds by caching a per-mode setpoint is unsound: a cold start has no value for the inactive mode, the inactive bound drifts stale while the active mode runs, and a write to the inactive bound has no OT register to land in. Only the single-entity model is achievable on this protocol.

### Alternative B: Derive `hvac_mode` from the master central-heating-enable bit

Set the mode from `ch_enable` (`MasterStatus & 0x01`, MsgID 0 HB bit 0) rather than from connection state. Rejected. `ch_enable` toggles with demand: it clears when the room is satisfied or idle. Driving the mode tile from it makes the tile flicker `heat <-> off` on every burner or compressor cycle, and a gas boiler idling through summer would read `off` while perfectly healthy. Defaulting to `heat` whenever the thermostat is connected matches the long-standing behaviour and is stable. This was validated against a real idle gas-boiler capture where `ch_enable` is clear the entire time.

### Alternative C: Derive `hvac_action` from the flame bit

Use the flame status bit (`SlaveStatus & 0x08`, MsgID 0 LB bit 3) to decide `heating`. Rejected. During a domestic-hot-water (DHW) draw the boiler fires (flame on) with central heating off, so flame would falsely paint the **room** climate as `heating` while the burner is actually making tap water. `hvac_action` reads the central-heating slave bit instead (`SlaveStatus & 0x02`), which is true only for space-heating demand. Confirmed in real DHW captures: flame on, central heating off.

### Alternative D: Do nothing (keep the heating-only entity)

Leave `modes` at `["off","heat"]`. Rejected: this is the exact #665 defect. Heat/cool systems would keep showing a heating thermostat while cooling, with the cooling setpoint mislabelled and no way to switch back. The OT bus already carries cooling correctly, so the firmware would be discarding information it has.

## Consequences

**Benefits**

- Heat/Cool systems (heatpumps, reversible units) get the correct mode, action, and setpoint in HA. The #665 user can finally see and trust the cooling state.
- Heating-only and gas-boiler installations are unchanged by construction: cooling bits stay clear, so `cool`/`cooling` never appear.
- The representation is spec-correct: mode and action come from defined OpenTherm status bits (MsgID 0), and the single setpoint stays the one the protocol actually carries (`TrSet`, MsgID 16).
- Both new topics are pass-through (the firmware emits the literal mode/action word), so the HA discovery templates stay trivial.

**Trade-offs**

- HA cannot offer a true `auto`/`heat_cool` tile for an OTGW. This is an inherent OpenTherm limitation (one setpoint, percentage cooling demand), documented here so it is not "fixed" later by someone who has not read the protocol.
- The mode is read-only (no `mode_command_topic`). A user cannot flip heat/cool from HA; only the thermostat can. This is deliberate (the master owns the switch and re-asserts every ~7 s); documented so it is not mistaken for a missing feature.
- Two more published topics on the hot publish path. Both are gated by the existing on-change plus heartbeat status-bit publishing (the `forcePublish || changed` guard in `publishHvacMode`/`publishHvacAction`), so the steady-state cost is negligible.

**Risks and mitigations**

- *Risk*: an implementer reads `hvac_action` from the flame bit (the "obvious" choice) and reintroduces the DHW false-positive. *Mitigation*: Alternative C records exactly why flame is wrong; the action derivation is pinned to the central-heating slave bit (`SlaveStatus & 0x02`).
- *Risk*: someone adds a `mode_command_topic` to "complete" the entity, creating a control that fights the master. *Mitigation*: the reflective decision and the ~7 s re-assert are stated in the Decision; the entity is intentionally mode-read-only.
- *Risk*: the four hvac states regress unnoticed in a future refactor. *Mitigation*: the design is validated against four real captures (gas-boiler idle, gas-boiler DHW, gas-boiler active space heating, heatpump cooling); a regression that paints DHW as heating, freezes the mode on cool, or hides cooling reproduces against those captures.

## Related Decisions

This design deliberately diverges from a downstream reference, recorded per the reference-precedence policy in `other-projects/CLAUDE.md`.

- **Home Assistant `opentherm_gw` integration (downstream interpretation, precedence level 3, not authority):** the vendored HA-core copy at `other-projects/core-dev/homeassistant/components/opentherm_gw/climate.py` handles cooling but is broken for this case. It sets `_attr_hvac_modes` to a **single-element** list per state (`[HVACMode.HEAT]`, `[HVACMode.COOL]`), and the idle `else` branch never resets the list, so after one cooling cycle the mode dropdown freezes on `cool` with no way back. Its HEATING branch also requires `ch_active and flame_on`, which a flameless heatpump never satisfies, so a heatpump that cooled once stays stuck on `cool`. Our design avoids both pitfalls: a **static** `["off","heat","cool"]` list, and mode/action from the enable and actual status bits rather than from flame. We do not copy `opentherm_gw`; the OpenTherm spec is the authority and this representation is spec-correct. (Confirmed by reading the vendored source; HA line numbers are version-volatile, the behaviours are the durable claim.)
- **ADR-031 (Two-Microcontroller Coordination Architecture):** the status bits consumed here are produced by the PIC/OT-frame parser that ADR-031's split defines. Depends on it.
- **ADR-042 (Streaming JSON I/O, No ArduinoJson):** the climate discovery payload (including the new `mode_stat_t`/`action_topic` bindings) is emitted through `streamClimateDiscovery` using the `MqttJsonWriter` streaming primitives this ADR defines; the payload shape changes here, the streaming mechanism is untouched. Builds on it.
- **ADR-041 (Just-In-Time Home Assistant MQTT Discovery):** the climate config is streamed on demand through the JIT discovery path; this ADR changes the climate payload only, not the discovery trigger. Builds on it.
- **ADR-071 (MQTT Discovery Topic Sibling-Suffix Shape):** the new `/hvac_mode` and `/hvac_action` value topics are flat siblings under `<pub>`, following the sibling-suffix topic shape this ADR established. Conforms to it.
- **ADR-081 (MQTT On-Change Publishing as the Default):** both new topics publish only on change plus the periodic force/heartbeat (`forcePublish || value != last`), exactly the on-change default this ADR sets. Conforms to it.
- **ADR-076 (MQTT Status Fan-Out, Drop Global Rate-Gate, Keep Per-Slot Heartbeat):** `publishHvacMode` is fanned out inside the master-status burst window (`beginStatusBurst`/`endStatusBurst`); the new topic rides the same status fan-out this ADR governs. Integrates with it.

Cross-line sibling (separate numbering, not part of this `otgw-1.x.x` tree's index):

- **2.0.0 `dev`-line sibling ADR-156** ports the same off/heat/cool decision to the 2.0.0 async firmware (tracked under TASK-939). The decision is coherent across both lines; only the file/line/field references differ (2.0.0 uses `MQTTHaDiscovery.cpp`, `state.otBus.bThermostatState`, and `isCoolingEnabled()`-style helpers, where 1.x uses `mqtt_configuratie.cpp`, `state.otgw.bThermostatState`, and inline status-bit masks).

## References

- GitHub issue #665 (Honeywell Round Modulation Heat/Cool on a heatpump; cooling invisible in HA): https://github.com/rvdbreemen/OTGW-firmware/issues/665
- 1.x implementation: commit `7b2d3cdd` ("feat(mqtt): unified heat/cool/off HA climate entity (cooling support, GH #665)"), released as `1.7.1-beta.2` on the `otgw-1.x.x` line. Design validated against four real captures (gas-boiler idle, gas-boiler DHW, gas-boiler active space heating, heatpump cooling).
- Firmware discovery site: `src/OTGW-firmware/mqtt_configuratie.cpp`, `streamClimateDiscovery` `climateIdx == 0` (`:2580` onward); `action_topic`/`action_template` (`:2602-2608`), `modes` (`:2652`), `mode_stat_t`/`mode_stat_tpl` (`:2659-2667`), `temp_stat_t`/`TrSet` (`:2689`), `temp_cmd_t`/`TT=` (`:2696-2701`), `max_temp` (`:2709`).
- Status-bit publish path: `src/OTGW-firmware/OTGW-Core.ino` `publishHvacMode` (`:1654`), `publishHvacAction` (`:1671`), called from `publishMasterStatusState` (`:1725`), `publishSlaveStatusState` (`:1770`), and on thermostat connect/disconnect (`:4137`). Status bytes `OTcurrentSystemState.MasterStatus`/`SlaveStatus`; thermostat liveness `state.otgw.bThermostatState`.
- OpenTherm setpoint semantics: MsgID 1 `TSet` (boiler water-control setpoint, not surfaced), MsgID 16 `TrSet` (room setpoint, the user target), MsgID 7 `CoolingControl` (percentage cooling demand). `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`.
- OpenTherm status/config semantics (same spec): MsgID 0 Status (HB master `CH enable` 0x01 / `DHW enable` 0x02 / `Cooling enable` 0x04, LB slave `CH active` 0x02 / `DHW active` 0x04 / `Flame` 0x08 / `Cooling active` 0x10); MsgID 3 Slave Configuration HB bit 7 "Heat/cool mode control" (clear: master may switch; set: slave switches).
- Downstream reference: `other-projects/core-dev/homeassistant/components/opentherm_gw/climate.py` (upstream: https://github.com/home-assistant/core/blob/dev/homeassistant/components/opentherm_gw/climate.py).
- Reference-precedence policy: `other-projects/CLAUDE.md`.

This ADR has **no `Enforcement` block**. The decision is a structural HA-representation choice that spans a firmware status-bit derivation, a reflective no-command-topic choice, and a discovery payload shape; its correctness is semantic (the mode/action precedence and the "mirror, do not command" rule), not a single mechanically-expressible code boundary, and there is no `evaluate.py` gate for it. This matches ADR-080's structural-versus-pattern split. Drift is caught at PR review against this ADR and the four reference captures, and in-session via `/adr-kit:judge`. If the maintainer wants the pre-commit judge to actively surface future climate-discovery changes for semantic review, this can be upgraded to an `llm_judge: true` Enforcement block.
