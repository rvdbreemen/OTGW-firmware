# ADR-156: Unified Off/Heat/Cool Home Assistant Climate Entity Derived from OpenTherm Status Bits

## Status

Accepted. Date: 2026-06-27.

**Decision Maker:** User: Robert van den Breemen (maintainer). The design shipped first on the 1.x maintenance line as `1.7.1-beta.2` (commit `7b2d3cdd`, GitHub issue #665) and was ported to the 2.0.0 `dev` line under TASK-939. The Proposed condition is now met: the port landed on `origin/dev` (commit `94890099`, alpha.279), the ESP32 build is SUCCESS with a fresh firmware.bin, `evaluate.py` reports Failed 0 (ESP abstraction clean), and implementation parity with the 1.x build (commit `7b2d3cdd`) is verified (both helpers evaluate disconnect-first, so the reviewer's asymmetry note does not apply). All four verification gates pass (adr-lint PASS strictly, adr-quality 0.96 grade A), so this ADR is now Accepted.

## Status History

status_history:
  - date: 2026-06-27
    status: Proposed
    changed_by: Agent
    reason: Initial decision record for the 2.0.0 port of the heat/cool HA climate representation (TASK-939); design validated on the 1.x line against real captures
    changed_via: adr-kit
  - date: 2026-06-27
    status: Accepted
    changed_by: User
    reason: 2.0.0 port landed on origin/dev (commit 94890099, alpha.279); ESP32 build SUCCESS with fresh firmware.bin, evaluate.py Failed 0 (ESP abstraction clean); implementation parity with 1.x build 7b2d3cdd verified (both helpers evaluate disconnect-first); four verification gates pass (adr-lint PASS strictly, adr-quality 0.96 grade A)
    changed_via: adr-kit

## Context

The OpenTherm (OT) gateway exposes a single Home Assistant (HA) MQTT `climate` entity ("Thermostat") for the room. On the 2.0.0 `dev` line that entity is heating-only. In `streamClimateDiscovery` (`MQTTHaDiscovery.cpp`, `climateIdx == 0`):

- `modes` is `["off","heat"]` (`MQTTHaDiscovery.cpp:2953`),
- `mode_stat_t` binds `<pub>/thermostat_connected` with template `ON -> heat / else off` (`:2960-2969`),
- `action_topic` binds `<pub>/ch_enable` with template `ON -> heating / else idle` (`:2901-2903`),
- the target setpoint is `TrSet` (MsgID 16, the room setpoint) on `temp_stat_t` (`:2991`),
- `max_temp` is `28` (`:3011`).

A user on GitHub issue #665 runs a Honeywell Round Modulation Heat/Cool thermostat on a heatpump that both heats and cools, with an OTGW in between. The gateway relays cooling correctly at the OpenTherm level: the master sets `cooling_enable` (MsgID 0 high byte, bit 2), the boiler/heatpump answers with the cooling status bit (MsgID 0 low byte, bit 4), and demand is carried as a percentage on MsgID 7 (CoolingControl). The defect is purely in the HA representation: with `modes` fixed at `["off","heat"]` there is no `cool` mode, so while the system cools, HA still shows a heating thermostat carrying the cooling setpoint, with no control to switch back. Cooling is invisible and the entity is misleading.

The firmware already decodes every bit this needs. `OTGW-Core.ino` exposes `isCoolingEnabled()` (`MasterStatus & 0x04`, `:1444`), `isCoolingActive()` (`SlaveStatus & 0x10`, `:1483`), `isCentralHeatingActive()` (`SlaveStatus & 0x02`, `:1471`), and `isFlameStatus()` (`SlaveStatus & 0x08`, `:1479`). The 2.0.0 line also tracks thermostat liveness separately as `state.otBus.bThermostatState` (`OTBustypes.h`). The gap is that none of these reach the climate discovery payload; the entity was wired to `thermostat_connected` and `ch_enable` only.

## Decision

Surface a single unified `off`/`heat`/`cool` climate entity, driven by two new firmware-computed MQTT topics, with mode switching kept reflective (the firmware mirrors the thermostat, it does not command it).

### Two new computed topics

The firmware computes and publishes two values from the OT status bits (in the `OTGW-Core.ino` publish path, alongside the existing status-bit fan-out):

1. **`<pub>/hvac_mode`** in `{off, heat, cool}`:
   - `cool` when the master `cooling_enable` bit is set (`isCoolingEnabled()`, MsgID 0 HB bit 2),
   - else `heat` when the thermostat is connected (`state.otBus.bThermostatState`),
   - else `off` (only when the thermostat is disconnected).
2. **`<pub>/hvac_action`** in `{off, idle, heating, cooling}`, evaluated **cooling-before-heating** so the precedence is unambiguous if both slave bits are ever set together:
   - `off` when the thermostat is disconnected,
   - else `cooling` when the slave cooling bit is set (`isCoolingActive()`, MsgID 0 LB bit 4),
   - else `heating` when the slave central-heating bit is set (`isCentralHeatingActive()`, MsgID 0 LB bit 1),
   - else `idle`.

### Discovery payload changes

In `streamClimateDiscovery` (`climateIdx == 0`):

- `modes` becomes `["off","heat","cool"]` (static, never a per-state single-element list),
- `mode_stat_t` becomes `<pub>/hvac_mode` with a pass-through template (the firmware already emits the literal mode word),
- `action_topic` becomes `<pub>/hvac_action` with a pass-through template,
- `max_temp` widens from `28` to `30` (cooling setpoints sit above the heating comfort band),
- the single setpoint stays `TrSet` (MsgID 16) on `temp_stat_t`; `temp_cmd_t` stays `<sub>/command` with `TT={{ value }}`. `TSet` (MsgID 1, the boiler water-control setpoint) is **not** surfaced as the user target.

### Mode is reflective: no `mode_command_topic`

The entity has no `mode_command_topic`. Heat/cool selection is owned by the thermostat (the OpenTherm master), not by the in-line gateway: the thermostat sets the `cooling_enable` bit (MsgID 0 HB bit 2) and re-asserts the MsgID 0 status on the bus roughly every 7 seconds. The OpenTherm MsgID 3 Slave Configuration flag (HB bit 7, "Heat/cool mode control") designates whether the master or the slave performs the heat/cool switching (clear: switching can be done by master; set: switching is done by slave); in neither case is the gateway the controller. The OTGW mirrors the master's mode into HA; it does not originate it. An HA-side mode command would fight the master and be overwritten on the next status cycle, so offering the control would be a lie. HA can still command the **setpoint** (`TrSet` via `TT=`); only the mode is read-only.

Heating-only and gas-boiler users are unaffected: their `cooling_enable`/cooling bits are never set, so `hvac_mode` never becomes `cool` and `hvac_action` never becomes `cooling`.

## Alternatives Considered

### Alternative A: Dual-setpoint `HVACMode.HEAT_COOL` (auto, with `target_temp_high` + `target_temp_low`)

Model the system as HA's auto/heat_cool mode with a high and low bound. Rejected as **infeasible against the OpenTherm protocol**, not merely undesirable. OpenTherm exposes exactly one mode-agnostic room setpoint (`TrSet`, MsgID 16); cooling is commanded as a percentage demand (MsgID 7 CoolingControl), never as a second temperature. The two bounds never coexist on the wire: in the real #665 cooling capture the heating setpoint (around 19 C) never appears while cooling. Faking the two bounds by caching a per-mode setpoint is unsound: a cold start has no value for the inactive mode, the inactive bound drifts stale while the active mode runs, and a write to the inactive bound has no OT register to land in. A four-angle workflow-feasibility analysis on the 1.x side concluded that only the single-entity model is achievable on this protocol.

### Alternative B: Derive `hvac_mode` from the master central-heating-enable bit

Set the mode from `ch_enable` (`isCentralHeatingEnabled()`, MsgID 0 HB bit 0) rather than from connection state. Rejected. `ch_enable` toggles with demand: it clears when the room is satisfied or idle. Driving the mode tile from it makes the tile flicker `heat <-> off` on every burner or compressor cycle, and a gas boiler idling through summer would read `off` while perfectly healthy. Defaulting to `heat` whenever the thermostat is connected matches the long-standing behaviour and is stable. This was validated against a real idle gas-boiler capture where `ch_enable` is clear the entire time.

### Alternative C: Derive `hvac_action` from the flame bit

Use the flame status bit (`isFlameStatus()`, MsgID 0 LB bit 3) to decide `heating`. Rejected. During a domestic-hot-water (DHW) draw the boiler fires (flame on) with central heating off, so flame would falsely paint the **room** climate as `heating` while the burner is actually making tap water. `hvac_action` reads the central-heating slave bit instead (`isCentralHeatingActive()`), which is true only for space-heating demand. Confirmed in real DHW captures: flame on, central heating off.

### Alternative D: Do nothing (keep the heating-only entity)

Leave `modes` at `["off","heat"]`. Rejected: this is the exact #665 defect. Heat/cool systems would keep showing a heating thermostat while cooling, with the cooling setpoint mislabelled and no way to switch back. The OT bus already carries cooling correctly, so the firmware is discarding information it has.

## Consequences

**Benefits**

- Heat/Cool systems (heatpumps, reversible units) get the correct mode, action, and setpoint in HA. The #665 user can finally see and trust the cooling state.
- Heating-only and gas-boiler installations are unchanged by construction: cooling bits stay clear, so `cool`/`cooling` never appear.
- The representation is spec-correct: mode and action come from defined OpenTherm status bits (MsgID 0), and the single setpoint stays the one the protocol actually carries (`TrSet`, MsgID 16).
- Both new topics are pass-through (the firmware emits the literal mode/action word), so the HA discovery templates stay trivial and the per-value flat-topic contract (ADR-101) holds.

**Trade-offs**

- HA cannot offer a true `auto`/`heat_cool` tile for an OTGW. This is an inherent OpenTherm limitation (one setpoint, percentage cooling demand), documented here so it is not "fixed" later by someone who has not read the protocol.
- The mode is read-only (no `mode_command_topic`). A user cannot flip heat/cool from HA; only the thermostat can. This is deliberate (the master owns the switch and re-asserts every ~7 s); documented so it is not mistaken for a missing feature.
- Two more published topics on the hot publish path. Both are gated by the existing on-change plus heartbeat status-bit publishing, so the steady-state cost is negligible.

**Risks and mitigations**

- *Risk*: an implementer reads `hvac_action` from the flame bit (the "obvious" choice) and reintroduces the DHW false-positive. *Mitigation*: Alternative C records exactly why flame is wrong; the action derivation is pinned to the central-heating slave bit.
- *Risk*: someone adds a `mode_command_topic` to "complete" the entity, creating a control that fights the master. *Mitigation*: the reflective decision and the ~7 s re-assert are stated in the Decision; the entity is intentionally mode-read-only.
- *Risk*: the four hvac states are only validated on the 1.x build. *Mitigation*: the 2.0.0 port (TASK-939) re-verifies parity with commit `7b2d3cdd` against the same four real captures (gas-boiler idle, gas-boiler DHW, gas-boiler active space heating, heatpump cooling) before this ADR flips to Accepted.

## Related Decisions

This design deliberately diverges from a downstream reference, recorded per the reference-precedence policy in `other-projects/CLAUDE.md`.

- **Home Assistant `opentherm_gw` integration (downstream interpretation, precedence level 3, not authority):** the vendored HA-core copy at `other-projects/core-dev/homeassistant/components/opentherm_gw/climate.py` handles cooling but is broken for this case. It sets `_attr_hvac_modes` to a **single-element** list per state (`[HVACMode.HEAT]` at `:78` and `:132`, `[HVACMode.COOL]` at `:136`), and the idle `else` branch (`:138`) never resets the list, so after one cooling cycle the mode dropdown freezes on `cool` with no way back. Its HEATING branch also requires `ch_active and flame_on` (`:129`), which a flameless heatpump never satisfies, so a heatpump that cooled once stays stuck on `cool`. Our design avoids both pitfalls: a **static** `["off","heat","cool"]` list, and mode/action from the enable and actual status bits rather than from flame. We do not copy `opentherm_gw`; the OpenTherm spec is the authority and this representation is spec-correct. (Confirmed by reading the vendored source; HA line numbers are version-volatile, the behaviours are the durable claim.)
- **ADR-098 (MQTT Discovery Topic Sibling-Suffix Shape):** the new `/hvac_mode` and `/hvac_action` value topics are flat siblings under `<pub>`, following the sibling-suffix topic shape this ADR established. Complements it.
- **ADR-101 (Flat Per-Value MQTT Topics Over Aggregated JSON):** each new topic carries one scalar word, with the HA discovery metadata on the config topic. Conforms to it.
- **ADR-084 (Generic OT-Bus State MQTT Topics):** the existing climate entity already reads generic-namespace state topics (`thermostat_connected`); the new topics extend the same namespace. Builds on it.
- **ADR-077 (Streaming MQTT HA Discovery Architecture):** the climate discovery payload (including the new `mode_stat_t`/`action_topic` bindings) is emitted through `streamClimateDiscovery` using the `MqttJsonWriter` + `measureMallocPublish` streaming primitives this ADR defines; the payload shape changes here, the streaming mechanism is untouched. Builds on it.
- **ADR-140 (Single-Device HA Discovery Topology):** the climate entity stays inside the single OTGW device; this ADR changes the climate payload only, not the device topology. Preserves it.
- **ADR-031 (Two-Microcontroller Coordination Architecture):** the status bits consumed here are produced by the PIC/OT-frame parser that ADR-031's split defines. Depends on it.

## References

- GitHub issue #665 (Honeywell Round Modulation Heat/Cool on a heatpump; cooling invisible in HA): https://github.com/rvdbreemen/OTGW-firmware/issues/665
- 1.x implementation: commit `7b2d3cdd`, released as `1.7.1-beta.2` on the `otgw-1.x.x` line. Design validated there against four real captures (gas-boiler idle, gas-boiler DHW, gas-boiler active space heating, heatpump cooling).
- 2.0.0 port: TASK-939 (`feat-2.0.0: port TASK-938`, unified heat/cool/off HA climate entity).
- Firmware discovery site (2.0.0): `src/OTGW-firmware/MQTTHaDiscovery.cpp`, `streamClimateDiscovery` `climateIdx == 0` (`:2876-3046`); `modes` (`:2953`), `mode_stat_t` (`:2960-2969`), `action_topic` (`:2901-2903`), `temp_stat_t`/`TrSet` (`:2991`), `max_temp` (`:3011`).
- Status-bit decoders (2.0.0): `src/OTGW-firmware/OTGW-Core.ino` `isCoolingEnabled()` (`:1444`), `isCoolingActive()` (`:1483`), `isCentralHeatingActive()` (`:1471`), `isFlameStatus()` (`:1479`); thermostat liveness `state.otBus.bThermostatState` (`src/OTGW-firmware/OTBustypes.h`).
- OpenTherm setpoint semantics: MsgID 1 `TSet` (boiler water-control setpoint, not surfaced), MsgID 16 `TrSet` (room setpoint, the user target), MsgID 7 `CoolingControl` (percentage cooling demand). `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`.
- OpenTherm status/config semantics (same spec): MsgID 0 Status (HB master `CH enable`/`DHW enable`/`Cooling enable`, LB slave `CH active`/`DHW active`/`Flame`/`Cooling active`); MsgID 3 Slave Configuration HB bit 7 "Heat/cool mode control" (clear: master may switch; set: slave switches), spec table at lines 1603-1620.
- Downstream reference: `other-projects/core-dev/homeassistant/components/opentherm_gw/climate.py:78,124-138,187` (upstream: https://github.com/home-assistant/core/blob/dev/homeassistant/components/opentherm_gw/climate.py).
- Reference-precedence policy: `other-projects/CLAUDE.md`.

This ADR has **no `Enforcement` block**. The decision is a structural HA-representation choice that spans a firmware status-bit derivation, a reflective no-command-topic choice, and a discovery payload shape; its correctness is semantic (the mode/action precedence and the "mirror, do not command" rule), not a single mechanically-expressible code boundary, and there is no `evaluate.py` gate for it. This matches ADR-080's structural-versus-pattern split and the same call made in ADR-155. Drift is caught at PR review against this ADR and the four reference captures, and in-session via `/adr-kit:judge`. If the maintainer wants the pre-commit judge to actively surface future climate-discovery changes for semantic review, this can be upgraded to an `llm_judge: true` Enforcement block.
