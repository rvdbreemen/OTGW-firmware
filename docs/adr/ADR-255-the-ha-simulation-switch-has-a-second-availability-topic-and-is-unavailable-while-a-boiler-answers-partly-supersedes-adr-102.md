---
id: "ADR-255"
title: "The HA simulation switch has a second availability topic and is unavailable while a boiler answers (partly supersedes ADR-102)"
status: "Proposed"
date: "2026-11-18"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-101"
  - "ADR-102"
  - "ADR-142"
  - "ADR-174"
topics:
  - "home assistant availability"
  - "sat simulation switch"
  - "mqtt discovery"
  - "availability list"
aliases:
  - "TASK-2014"
  - "avty_mode all"
  - "sat/simulation_available"
  - "simulation switch bounces back"
components:
  - "sat/simulation_available"
  - "streamSatBoolSwitch availSub"
symbols:
  - "satPublishCoreState"
  - "streamSatBoolSwitch"
  - "satBoilerHardwarePresent"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-255 The HA simulation switch has a second availability topic and is unavailable while a boiler answers (partly supersedes ADR-102)

## Status

Proposed, 2026-11-18.

**Decision Maker:** User: Robert van den Breemen (maintainer). Option "A", Discord DM 1572560403387523154 of 2026-11-18, a reply to the coordinator's A/B/C question 1572511441158934619, relayed into TASK-2014.

## Status History

```yaml
status_history:
  - date: 2026-11-18
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal, written after the maintainer chose option A for TASK-2014; the decision is Robert's, the record is the agent's
    changed_via: adr-kit
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-102
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-142
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-174
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-101
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) has a simulation mode that runs SAT against a simulated boiler. Enabling it is refused while a real boiler answers on the bus: `updateSetting("SATsimulation")` checks `satBoilerHardwarePresent()` and keeps `settings.sat.bSimulation` false (`src/OTGW-firmware/settingStuff.ino:1242-1245`, TASK-795; `SATcontrol.ino:3095`). The reason goes only to telnet and to the web UI's WebSocket event (`settingStuff.ino:1243-1244`).

Home Assistant (HA) discovers a switch "SAT Simulation" (`MQTTHaDiscovery.cpp:3515`, through `streamSatBoolSwitch()`, `:3383`) with command topic `sat/simulation` (`MQTTstuff.ino:744`) and state topic `sat/simulation_enable` (`SATcontrol.ino:5764`). On a unit with a boiler, a tester switched it on, the firmware refused, published `false`, and HA showed the switch jump back to off within a second, without a reason (thaghostnl, #alpha-testing 2026-11-17, TASK-2014). Robert agreed with the report.

The web UI already handles this: REST `sim_available` is `!boilerHwPresent` (`SATcontrol.ino:4989`), and `satRenderSim()` disables the switch while it is false and simulation is off (`data/v2.js:7588-7592`). HA gets no such signal.

Every HA entity today has one availability topic, the device LWT (last will and testament) base topic `ctx.mqttPubTopic` (`kAvtyT` at `MQTTHaDiscovery.cpp:2503`, `:2622` and every other composer). Accepted ADR-102 fixed a defect where OpenTherm bus liveness was written to that base topic, so every entity flapped; its decision states that HA entity availability must reflect only the ESP to MQTT link. ADR-142 proposed a second, per-entity availability topic with `availability_mode: all` for unsupported OpenTherm message IDs; it was closed as Rejected on 2026-08-25 because it was never implemented and was not expected to be, not because of a technical objection (ADR-142 Status).

## Decision

**User Decision:** the HA "SAT Simulation" switch gets a second availability topic, `sat/simulation_available`, combined with the device LWT through `availability_mode: all`, so HA shows the switch unavailable while enabling simulation would be refused; every other entity keeps the single LWT availability of ADR-102.

The firmware publishes `sat/simulation_available` retained as `"true"` / `"false"`, with the rule of the web UI switch: `settings.sat.bSimulation || !satBoilerHardwarePresent()`. It is `"false"` only while a boiler answers and simulation is off; while simulation is on it stays `"true"`, so the switch can always be switched off. It is published by the SAT core-state publisher, which runs whether SAT is enabled or not (`satPublishCoreState()`, `SATcontrol.ino:5103`), with an on-change shadow that `satRepublishCoreState()` (`:5136`) re-arms on a full republish (ADR-174).

The simulation switch's discovery replaces its `avty_t` with an availability list:

```json
"avty": [
  {"t": "<pub>"},
  {"t": "<pub>/sat/simulation_available", "pl_avail": "true", "pl_not_avail": "false"}
],
"avty_mode": "all"
```

The first entry is the device LWT with HA's default `online` / `offline` payloads, unchanged. HA core expands abbreviations inside every availability list entry (`homeassistant/components/mqtt/discovery.py:215-219` on the HA `dev` branch, read 2026-11-18; `abbreviations.py` maps `t`, `pl_avail`, `pl_not_avail` and `avty_mode`).

This partly supersedes ADR-102: its rule stays for the base topic and for every other entity; for this one switch, availability also reflects whether the action it offers can succeed.

## Decision Contract

### Must

- Keep the base topic owned by the MQTT birth and LWT only (ADR-102); `check_adr102_otbus_liveness_topic` in `evaluate.py` stays green.
- Publish `sat/simulation_available` retained, on change, from a publisher that runs while SAT is off, and re-arm it on a full republish.
- Use the same rule as the web UI switch: unavailable only while a boiler answers and simulation is off.
- Keep the TASK-795 refusal in `updateSetting()`: the availability is the hint, the refusal is the guard.

### Must Not

- Must not write anything other than birth/LWT to the base topic.
- Must not give a second availability topic to any other entity without its own ADR.
- Must not hide the switch by withholding or emptying its discovery config.

### Exceptions

- None.

### Verification

- A host test that compiles the real `satPublishCoreState()` / `satRepublishCoreState()` and the real `streamSatSwitchDiscovery()` (planned `tests/test_ha_sat_sim_availability.py`, TASK-2014): the topic value for boiler present / absent / simulation on, the republish, the availability list on switch 5, and the unchanged single `avty_t` on the other switches. Red on the code before TASK-2014, green on the fix.
- A bench run in HA: with a boiler answering (or REST `force_boiler_present`, `restAPI.ino`), the switch shows unavailable; without, it is available again.

## Alternatives Considered

- **Keep the switch available and add a `sat/simulation_blocked` problem binary sensor (option B, "C+").** Fully inside ADR-102 and in an existing table pattern (`SAT_BIN` rows, `MQTTHaDiscovery.cpp`). Not chosen by the maintainer: the switch still jumps back; HA only shows the reason next to it. The work is parked in `RvdB/OTGW-worktree-leftovers/wt-95-2014-cplus-20261118/`.
- **Do not announce the switch while a boiler answers.** Boiler presence changes at runtime and is not known yet at the first discovery after boot; removing and re-adding needs empty retained configs (ADR-093 style) and loses the entity's HA customisation. Rejected for the same reasons ADR-142 rejected its alternative A.
- **Do nothing.** The refusal is correct, but the switch looks broken in HA; the reporter and the maintainer asked for a change.

## Consequences

**Positive:**

- HA shows the switch as unavailable while it cannot work, the same as the web UI: no click that bounces back.
- The device LWT keeps its meaning for all entities; only one switch gets an extra condition.
- One small retained topic (`"true"`/`"false"`), published on change: no measurable MQTT load.

**Negative:**

- An exception to ADR-102's general rule: availability of one entity now also says "the action is not possible now". Mitigation: the Decision Contract limits it to this switch; any other use needs its own ADR.
- A broker that loses retained messages leaves the switch unavailable until the next publish of the topic. Mitigation: the on-change shadow publishes at connect, and the ADR-174 republish re-arms it when HA comes back online.
- Right after boot, until the first boiler reply, the topic reads `"true"`; a click in that window is still refused by TASK-795. Accepted: the refusal stays the guard.
- Older HA versions: the availability list is documented in the current HA MQTT docs and handled in current HA core (References); which HA version introduced it was not checked here, so a very old HA may ignore the list. Not verified against an older HA.

## Open Questions

- None.

## Related Decisions

- **ADR-102 (HA entity availability reflects the MQTT link, not OpenTherm bus liveness):** partly superseded for the SAT simulation switch only; the base-topic rule and its gate stay.
- **ADR-142 (Rejected, availability list for unsupported MsgIDs):** same mechanism. That ADR was closed because it was never implemented, not on a technical objection; this ADR applies the mechanism to one entity with a known, local condition, instead of to every OT entity.
- **ADR-174 (republish on-change state when HA comes back online):** the new topic's shadow is re-armed by `satRepublishCoreState()`.
- **ADR-101 (flat per-value MQTT topics):** `sat/simulation_available` is one flat scalar topic.

## References

- TASK-2014 (backlog), report by thaghostnl in #alpha-testing, 2026-11-17 (https://discord.com/channels/812969634638725140/1514720723980259460/1572349389857361982).
- `src/OTGW-firmware/settingStuff.ino:1242-1245`, `SATcontrol.ino:3095`, `:4989`, `:5103`, `:5136`, `:5764`, `MQTTHaDiscovery.cpp:2503`, `:3383`, `:3515`, `MQTTstuff.ino:744`, `data/v2.js:7588-7592` (origin/dev, 2026-11-18).
- Home Assistant MQTT discovery: https://www.home-assistant.io/integrations/mqtt/ (abbreviations, availability); HA core `homeassistant/components/mqtt/discovery.py` and `abbreviations.py` (dev branch, read 2026-11-18).

## Enforcement

No declarative rule: the decision is verified by the TASK-2014 host test named under Verification, which compiles the real publisher and discovery compose, and the ADR-102 gate `check_adr102_otbus_liveness_topic` keeps the base topic clean.
