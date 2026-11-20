---
id: "ADR-248"
title: "The Home Assistant climate entity follows SAT while SAT is enabled, with a fixed discovery and routing in the firmware"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-101"
  - "ADR-174"
  - "ADR-179"
topics:
  - "home-assistant"
  - "mqtt"
  - "discovery"
  - "climate-entity"
  - "sat"
  - "presets"
aliases:
  - "HA thermostat card follows SAT"
  - "SAT climate entity"
  - "climate_target topic"
  - "TT refused while SAT is enabled"
components:
  - "HA climate discovery (Thermostat entity)"
  - "SAT climate bridge topics"
symbols:
  - "streamClimateDiscovery"
  - "kSatMqttCmds"
  - "satHandleTargetTemp"
  - "satHandlePreset"
  - "satOwnsControlSetpoint"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-248 The Home Assistant climate entity follows SAT while SAT is enabled, with a fixed discovery and routing in the firmware

## Status

Accepted, 2026-11-15.

**Decision Maker:** User: Robert van den Breemen (maintainer), following Sergeant D's proposal (docs/sat/sergeant-d-corrections.md entries 351, 352, 353).

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal (TASK-1979)
    changed_via: adr-kit
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-179
    changed_via: adr-kit lifecycle
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-174
    changed_via: adr-kit lifecycle
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-101
    changed_via: adr-kit lifecycle
  - date: 2026-11-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Robert accepted (Discord DM 1571639116955914493, 'A' to accept question 1571634712953225340); Sergeant D's answer 18 (entry 371) agrees
    changed_via: adr-kit lifecycle
```

## Context

The gateway publishes one Home Assistant (HA) MQTT climate entity, the "Thermostat"
(`src/OTGW-firmware/MQTTHaDiscovery.cpp:3023-3197`, `climateIdx == 0`). Its current
temperature is `<pub>/Tr` (:3126), its target is `<pub>/TrSet` (:3136), and setting a
temperature on the HA card publishes `TT={{ value }}` to `set/<node>/command` (:3148). It
has `modes` off/heat/cool (:3100) without a mode command, no presets, and SAT data only as
`json_attributes_topic` `<pub>/sat/climate_attributes` (:3184).

While SAT (Smart Autotune Thermostat) is enabled, SAT owns the control setpoint and the
gateway refuses `TT=` from MQTT and REST (ADR-179, keyed on `settings.sat.bEnabled`). So with
SAT on, the HA thermostat card shows the thermostat's values and its target field does
nothing. A tester asked for a climate entity with presets, target and current temperature
(indigo_light, #beta-testing 2026-11-15, message 1571549406790946897). Robert asked that the
climate entity has the full web UI feature set while SAT is enabled
(1571549972367810602). Sergeant D proposed using the existing entity, with two cases: a
physical thermostat attached (it stays in step through SAT's follow-thermostat and push
setpoint options) and standalone operation (entry 351). Robert chose that approach (entry 352)
and approved the concrete proposal posted as 1571560639548104886 (entry 353).

The firmware already has SAT inputs: `set/<node>/sat/target` (`MQTTstuff.ino:675`,
`satHandleTargetTemp`, `SATcontrol.ino:3688-3710`, which selects a preset when the value equals
one, as thermo-nova's `climate.py:557-568` does) and `set/<node>/sat/preset`
(`MQTTstuff.ino:686`, `satHandlePreset`, `SATcontrol.ino:2284`), with state on `sat/target`
(`SATcontrol.ino:5106`) and `sat/preset` (`SATcontrol.ino:4988`, value `none` when no preset is
active). SAT regulates on `satGetRoomTemp()`, published as `sat/room_temp`
(`SATcontrol.ino:5199`), which can be an external or BLE (Bluetooth Low Energy) sensor rather
than MsgID 24.

## Decision

The existing climate.Thermostat entity keeps its `uniq_id` and its discovery payload is fixed;
the firmware routes the card's target command and the card's state by `settings.sat.bEnabled`,
so the card is SAT's thermostat while SAT is enabled and today's thermostat card while it is off.

1. **Target command.** `temp_cmd_t` becomes `set/<node>/sat/climate_target` and
   `temp_cmd_tpl` becomes `{{ value }}` (was `TT={{ value }}`). A new `kSatMqttCmds` row
   handles it: while `satOwnsControlSetpoint()` it calls `satHandleTargetTemp(value)`; otherwise
   it queues `TT=<value>` through `addCommandToQueue()`, which is today's behaviour.
2. **Target and current temperature state.** Two new flat scalar topics (ADR-101):
   `<pub>/sat/climate_target` carries `settings.sat.fTargetTemp` while SAT is enabled and the
   thermostat setpoint (TrSet) while off; `<pub>/sat/climate_current` carries
   `satGetRoomTemp()` while enabled and the room temperature (Tr) while off. `temp_stat_t` and
   `curr_temp_t` point at them.
3. **Presets.** `preset_modes` `["comfort","eco","away","sleep","activity","home"]`, together
   with `preset_mode_command_topic` `set/<node>/sat/preset` and `preset_mode_state_topic`
   `<pub>/sat/preset` (both existing). `none` is not listed, because HA refuses it in
   `preset_modes` and resets the preset when it arrives on the state topic
   (`other-projects/core-dev/homeassistant/components/mqtt/climate.py:213-216`, `:709-710`), and
   `preset_modes` with its command topic is required as a pair (`vol.Inclusive`, `:300-306`).
4. **Range.** `min_temp` becomes 5 (was 12, `MQTTHaDiscovery.cpp:3156`), because SAT accepts
   5-30 (`SATcontrol.ino:3693`) and the Activity preset defaults to 10 C (`SATtypes.h:512`).
5. **Publishing.** The two new state topics have their own publisher on the loop task, outside
   `satPublishMQTT()` (which returns while SAT is off, `SATcontrol.ino:5023`) and outside
   `processOT()` (which holds the OT lock), inside the ADR-088 status-burst wrapper. They publish
   on change, are forced on every SAT enable/disable through a flag set in
   `updateSetting("SATenabled")` (`settingStuff.ino:1096-1115`, reached from MQTT, REST and the
   web UI), and are included in `requestMQTTRepublishAll()` (ADR-174).

## Decision Contract

### Must

- Keep the climate entity's `uniq_id` and HA device (ADR-140) unchanged.
- Route the target command by `satOwnsControlSetpoint()` (ADR-179): SAT target while SAT is enabled, `TT=` otherwise.
- Publish `sat/climate_target` and `sat/climate_current` also while SAT is disabled, and force both on every SAT enable/disable.
- Keep `none` out of `preset_modes` and publish `preset_modes` only together with its command topic.

### Must Not

- Republish or change the climate discovery payload when SAT is toggled.
- Send `TT=` from the climate entity while SAT is enabled.
- Publish the new topics from `processOT()` or outside the ADR-088 burst wrapper.

### Exceptions

- The card's mode (off/heat/cool) stays read-only; switching SAT on or off from the card is out of scope (SAT keeps its own switch entity).

### Verification

- A host test that compiles the real `streamClimateDiscovery()` compose and checks `temp_cmd_t`, `temp_cmd_tpl`, `temp_stat_t`, `curr_temp_t`, `min_temp` and the three preset fields; it fails on the code before TASK-1979.
- A host test on the `climate_target` handler: SAT enabled calls the SAT target path, SAT disabled queues `TT=<value>`.
- Bench on both board types and an HA screenshot of the card with SAT on and off (TASK-1979 ACs).

## Alternatives Considered

- **A separate SAT climate entity next to the existing one.** The existing entity stays untouched, but it does nothing while SAT is on, SAT users must re-point dashboards and automations to a new entity id, and there are two thermostat cards. Rejected by Robert's choice of Sergeant D's approach (entry 352): "minimum hassle for the user" (Sergeant D, 1571557990392402042).
- **The same entity, with a discovery payload that changes on every SAT toggle.** Same entity id, but the card's features change under the user, a missed republish leaves a wrong card, and a toggle today republishes no discovery (`satHandleEnabled`, `SATcontrol.ino:3736-3744`, through `updateSetting`, `settingStuff.ino:1096-1115`), so a new republish path would be needed. Rejected: a fixed payload with routing in the firmware needs no discovery change at all.
- **Do nothing.** The card keeps sending `TT=`, which is refused while SAT is enabled (ADR-179), so the card cannot set anything for a SAT user. Rejected by Robert (1571549972367810602).

## Consequences

**Positive:**

- Current HA users keep the same entity id; dashboards and automations that call `climate.set_temperature` keep working, and with SAT off nothing changes on the bus (`TT=` as before).
- With SAT on, the card sets SAT's target and presets like the web UI and shows the room temperature SAT regulates on.
- With follow thermostat and push setpoint on, the wall thermostat and the card stay in step through SAT (entry 2): a wall-thermostat change becomes SAT's target (`SATcontrol.ino:2428-2436`), a card change is pushed with `TC=` (`SATcontrol.ino:8051-8055`).

**Negative:**

- Two new MQTT state topics and one command topic; `docs/api/MQTT.md` and the HA section of the manual change. Mitigation: documented in TASK-1979, scalar topics only.
- With SAT off, the presets are still on the card; choosing one sets SAT's stored preset and target (`satHandlePreset` has no enabled check) while the card's target shows the thermostat setpoint. Mitigation: documented as known behaviour; the preset applies once SAT is enabled.
- Some thermostats (the Honeywell Round) do not show an accepted override in their own setpoint (MsgID 16), so the wall display can differ from the card (entry 2). Mitigation: named in the user documentation.
- A stale card value after a SAT toggle if the forced publish is lost. Mitigation: the force flag plus `requestMQTTRepublishAll()`, covered by the host test and the bench.

## Open Questions

- [x] What Sergeant D meant by "SAT should be displayed as `OTGW_Thermostat`" (entry 351): the entity's display name, or the gateway taking the thermostat role. Queued as Q-1979a in docs/sat/open-questions.md; this ADR keeps the name "Thermostat" until it is answered. — **Answered 2026-11-15 by User: Robert van den Breemen:** Keep the name 'Thermostat'. Decided by the coordinator under Robert's directive 2026-11-15 (CLI) 'Do not wait for Sergeant D. Finish everything that can be finished.' (Q-1979a option B, our recommendation); Sergeant D is asked in the same batch and may overrule, which then becomes a follow-up task.

## Related Decisions

- **ADR-179 (Name the owner of the OpenTherm control setpoint while SAT is enabled)**: the routing key; this ADR makes the climate entity respect that ownership instead of sending a refused `TT=`.
- **ADR-174 (Republish on-change gated MQTT state when Home Assistant comes back online)**: the new state topics join `requestMQTTRepublishAll()`.
- **ADR-101 (Flat per-value MQTT topics)**: the new topics are scalar; `sat/climate_target` and `sat/climate_current` have a meaning that depends on the SAT state, stated here.
- **ADR-077 (Streaming MQTT HA discovery architecture)**: the change stays inside `streamClimateDiscovery()`.
- **ADR-140 (Single-device HA topology)**: same device, same entity.
- **ADR-088 (MQTT status-burst windowing)**: the new publisher runs inside the burst wrapper.

## References

- TASK-1979 (research notes, plan, review findings).
- docs/sat/sergeant-d-corrections.md entries 2, 311, 351, 352, 353.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp:3023-3197`; `src/OTGW-firmware/MQTTstuff.ino:673-690`; `src/OTGW-firmware/SATcontrol.ino:2284`, `:3688-3710`, `:4988`, `:5023`, `:5106`, `:5199`; `src/OTGW-firmware/settingStuff.ino:1096-1115`; `src/OTGW-firmware/SATtypes.h:508-513`.
- thermo-nova `custom_components/sat/climate.py:101`, `:557-568`, `:604-624`, `:858-865` at 7beaaf5.
- Home Assistant MQTT climate: https://www.home-assistant.io/integrations/climate.mqtt/ and `other-projects/core-dev/homeassistant/components/mqtt/climate.py:213-216`, `:300-306`, `:709-710`.

## Enforcement

No declarative rule yet: the decision is verified by the TASK-1979 host tests named under Verification, which compile the real discovery compose and command handler. Once they exist, they are the gate this ADR references.
