---
id: "ADR-294"
title: "Frost guard: with SAT off and no thermostat on the bus the gateway sends CS=30 with CH=1 while the measured room is at or below 6 C, until 7 C"
status: "Accepted"
date: "2026-11-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "frost protection"
  - "sat off"
  - "no thermostat"
aliases:
  - "frost guard"
  - "vorstbeveiliging"
components:
  - "SAT frost guard"
symbols:
  - "satFrostGuardDecide"
  - "satFrostGuardService"
  - "satNoteExternalCS"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-294 Frost guard: with SAT off and no thermostat on the bus the gateway sends CS=30 with CH=1 while the measured room is at or below 6 C, until 7 C

## Status

Accepted, 2026-11-26.

## Status History

```yaml
status_history:
  - date: 2026-11-21
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-26
    status: Accepted
    changed_by: "Robert van den Breemen (decision, interactive answer to the coordinator, relayed), recorded by Agent: Claude (otgw-architect)"
    reason: "Accepted: decision Robert van den Breemen, 2026-11-26 21:27 (date on the recording machine), verbatim 'Accepteren (Recommended)' (interactive answer to the coordinator, relayed; question: frost guard with SAT off, no thermostat, room <= 6 C gives CS=30 and CH=1, shipped since beta.438), recorded by Agent: Claude (otgw-architect); four gates passed in adr-kit"
    changed_via: adr-kit lifecycle
```

## Context

Terms used below: SAT (Smart Autotune Thermostat), PIC (the Classic gateway's Microchip microcontroller), BLE (Bluetooth Low Energy), MQTT (Message Queuing Telemetry Transport).

With SAT switched off and no thermostat behind the gateway, nothing asks the boiler for heat. On OT-Direct the gateway's own MsgID 0 carries CH enable off without a CS= override (OTDirect.ino buildStatusRequest(), TASK-1806, "like the PIC"); on the Classic the PIC's standalone status sets CH enable only while a CS= is in force (gateway.asm 6.6 readstatus :2712-2742, NoThermostat set by Disconnect :1770-1789). A house with such a gateway can freeze. Robert decided that the gateway protects against frost in that case, on both boards, in 2.0.0 (entries 460 and 461 in the private SAT register, Q-456a = B). Alex Wijn's develop branch has the same idea and is a reference only (ADR-292): ba515c8 demand_control.py:610-622 (on at room <= threshold, off at >= threshold + 1.0, no room temperature means off) and :815-828 (HVAC off plus frost gives a demand with setpoint max(minimum, 30)).

## Decision

While SAT does not control the setpoint, no thermostat frame was seen on the bus for 30 s (state.otBus.bThermostatState, the same window as the thermostat_connected entity) and a measured room temperature is at or below 6.0 C, the gateway sends CS=30.0 and CH=1 through addCommandToQueue() and repeats CS=30.0 every 30 s; at 7.0 C or more, when the room temperature is lost, or when a thermostat appears it sends CS=0. One output serves both boards: the PIC and OT-Direct (the MsgID 1 override, otdEffectiveMasterFlags) both turn CS= plus CH=1 into CH enable on with a 30 C flow setpoint.

## Decision Contract

### Must

- The rule is the pure function satFrostGuardDecide() between the `// --- satFrostGuard` markers in SATcontrol.ino; the thresholds 6.0 / 7.0 C are fixed, without a setting, and live only there (Sergeant D, entry 485, Q-460a answered A).
- The room temperature comes only from measured sources: the chosen BLE sensor and the external MQTT sensor (entries 457/458; an explicit primary of one of them goes first). Averaged areas and the thermostat's MsgID 24 are left out: the guard only acts with no thermostat on the bus, and an average over areas can hide a cold room (our choice, not in the register). Simulation, the fallback estimate and the Summer Simmer value do not count, and the guard does not change _sat_roomSrc.
- The guard's commands are marked satOwn and stay out of SAT's CS cache (state.sat.fLastSentCS).
- A user CS= (REST, MQTT, telnet through addCommandToQueue()) keeps priority: 8 C or more for 130 s after it was sent, below 8 C until a CS=0. SAT or a user CS= taking over ends the guard without a CS=0.
- The guard waits 60 s after boot, stays off in monitor mode, during a flash and while SAT's hand-back is pending.
- /api/v2/sat/status shows frost_guard_active; the enter and exit are one debug line each.

### Must Not

- No new MQTT topic or Home Assistant entity for this (YAGNI; a request from Robert changes that).
- No CS=0 for a setpoint someone else owns.

### Exceptions

- On the Classic a CS= that OTmonitor sends through port 25238 does not pass addCommandToQueue() (byte-transparent bridge with SAT off, ADR-217), so the guard cannot see it and overwrites it every 30 s while the room is at or below the threshold.

### Verification

- tests/test_sat_frost_guard.py compiles the real marked block (red on the code before TASK-3021: the block does not exist) and checks the wiring in satControlLoop(), the room helper and addCommandToQueue().
- Bench on OT-Direct master mode and on a Classic + S3 (otgw-bench-tester), room fed over MQTT at 5.0 and 7.5 C.

## Alternatives Considered

- **OT-Direct only, through its own MsgID 0 bit 0 and TSet.** Rejected by Robert's entry 461: the Classic needs the same protection, and a second output path is more code for the same effect.
- **Use satGetRoomTemp() as it is.** Rejected: it reports simulation and the fallback estimate as a room temperature and writes the source that sat/room_temp_source publishes.
- **A user setting for the thresholds now.** Rejected by Sergeant D (entry 485): fixed 6 C / 7 C, no setting; the values follow Alex's default (const.py frost threshold 6.0).
- **Do nothing (today).** Rejected: a gateway without a thermostat and with SAT off leaves the house unprotected.

## Consequences

**Positive:**

- A gateway without a thermostat protects the house against frost also when SAT is off, on both boards, with one rule and one output.

**Negative:**

- The gateway heats on its own: a user who wants the boiler off must use a CS= of their own or a room temperature above 6 C. Mitigation: the decision is visible (frost_guard_active, debug lines).
- A user CS=0 while the guard is on is overruled by the next 30 s repeat (frost wins).
- The PIC basis (CH enable only with a CS= in force) is code reading of gateway.asm, not yet bench-verified.

## Open Questions

- [x] Q-460a: fixed 6.0 / 7.0 C or a user setting: answered A by Sergeant D (entry 485, 2026-11-25), fixed, no setting.
- [x] Implementation choices that are not in the register (ours, listed in the code and in TASK-3021): monitor mode does nothing; 60 s boot grace; a user CS= of 8 C or more wins for 130 s; no action while a hand-back is pending or while flashing; evaluation every 5 s; CS=30 repeated about every 30 s; the room chain above. — **Answered 2026-11-26 by Robert van den Breemen (decision, interactive answer to the coordinator, relayed), recorded by Agent: Claude (otgw-architect):** Not a separate question to Robert: these are our implementation choices, listed openly. They are taken as part of the ADR that Robert accepted as written (verbatim 'Accepteren (Recommended)', 2026-11-26 21:27 by date on the recording machine, interactive answer to the coordinator, relayed; not seen by otgw-architect). That reading is ours (coordinator and otgw-architect), not his words. A change to any of them goes through a new task.

## Related Decisions

- **ADR-292**: Alex Wijn's SAT is a reference, the recorded decisions lead.
- **ADR-235**: the 30 s CS= repeat the guard copies.
- **ADR-217**: why port 25238 commands are invisible to the guard on the Classic.
- **ADR-293**: the room temperature sources the guard reads.

## References

- TASK-3021; entries 460, 461, 485 (decisions register).
- src/OTGW-firmware/SATcontrol.ino (satFrostGuardDecide, satFrostGuardService, satFrostRoomTemp), OTGW-Core.ino addCommandToQueue(), OTDirect.ino buildStatusRequest().
- other-projects/otgw-6.6/gateway.asm :1770-1816 (Disconnect, StandAloneMsg), :2490-2495 (gateway requests print as R/A, not T), :2712-2742 (readstatus).
