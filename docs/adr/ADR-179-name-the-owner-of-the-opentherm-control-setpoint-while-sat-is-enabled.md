---
id: "ADR-179"
title: "Name the owner of the OpenTherm control setpoint while SAT is enabled"
status: "Proposed"
date: "2026-09-22"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "control setpoint ownership"
  - "msgid 1 tset"
  - "sat arbitration"
  - "otdirect write arbitration"
aliases:
  - "TSet owner"
  - "MsgID 1 owner"
  - "satOwnsControlSetpoint"
components:
  - "satOwnsControlSetpoint()"
  - "OTDirect MsgID 1 writers"
symbols:
  - "satOwnsControlSetpoint"
  - "satCommandInFlight"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-179 Name the owner of the OpenTherm control setpoint while SAT is enabled

## Status

Proposed, 2026-09-22.

## Status History

```yaml
status_history:
  - date: 2026-09-22
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

MsgID 1 (TSet, the boiler control setpoint) is the single most consequential value the
firmware writes. Until now no document said who owns it, and five code paths could write
it independently.

Reported on Discord (#dev-sat-mqtt, 2026-05-29) by sergeantd: *"When SAT is enabled,
ONLY SAT should drive TSet. Otherwise a conflict will occur."* The visible symptom was a
45 <-> 10 flip-flop every 60 seconds. TASK-761 fixed that same day by gating OTDirect's
heating-curve writer on `state.sat.bActive` (`OTDirect.ino:2072-2085`). The symptom went
away; the rule did not exist.

A verification pass over dev HEAD (2026-09-22), including an adversarial completeness
check that grepped every `setOverride(1` / `updateWriteCache(1` / `enqueueWriteCommand(1`
site, found the writer set is exactly five and that four remained ungated. In order of
how easily they fire:

1. **Thermostat-timeout setback** (`OTDirect.ino:2470-2475`). `settings.otd.bFailSafe`
   defaults true and `iSetbackTimeout` defaults 30 s (`OTDirecttypes.h:110,113`), and
   gateway is the default mode. A quiet thermostat hands TSet to a 16 C setback with no
   external actor involved.
2. **`CS=` has no gate** (`OTDirect.ino:2620-2641`), reachable from MQTT `ctrlsetpt` and
   the raw `command` topic, `POST /api/v2/otgw/commands`, the port-25238 bridge and
   telnet. `addCommandToQueue()` (`OTGW-Core.ino:3550-3561`) arbitrates nothing, so the
   last writer wins.
3. **Refresh versus expiry.** SAT re-sends CS on change or every `SAT_CMD_REFRESH_MS`
   (300 s, `SATcontrol.ino:312`); OTDirect expires the override after
   `OT_CSC2_EXPIRY_MS` (60 s, `OTDirect.ino:456`). `otCSLastCommandMs` is never stamped
   by the scheduler, so the expiry really fires while SAT is deliberately holding a
   stable setpoint — during the ADR-150 cold cutoff and while DHW is active.
4. **Silent handover on a safety trip.** `satDisable()` clears `state.sat.bActive` and
   leaves `settings.sat.bEnabled` true (`SATcontrol.ino:1853-1873`), so a gate keyed on
   `bActive` re-arms the heating curve while the user still believes SAT is in charge.
5. **Master-mode thermostat WRITE_DATA** (`OTDirect.ino:2523-2527`), ungated.

One question had to be answered before any fix, because it decides whether MsgID 1
arbitration is even sufficient: does SAT's `CH=` reach the boiler on pass-through
thermostat frames? It does not, and the real answer is worse. `otMasterStatusFlags` is
read at exactly one bus-facing site, `buildStatusRequest()` (`OTDirect.ino:733`), and
there is no `setOverride(0, ...)` anywhere, so a thermostat's own MsgID 0 is relayed
unmodified. But `scheduleMasterRequest()` runs in gateway mode too — only monitor mode
is excluded (`OTDirect.ino:2015`) — so the boiler receives the thermostat's MsgID 0 and
the gateway's MsgID 0 interleaved, each carrying a different CH-enable bit. Whose bit
applies depends on which frame arrived last.

## Decision

While SAT is **enabled**, SAT owns MsgID 1. Ownership is expressed by one predicate,
`satOwnsControlSetpoint()`, keyed on `settings.sat.bEnabled` rather than
`state.sat.bActive`, and every competing writer consults it. The thermostat-timeout
setback is the single documented exception and deliberately outranks SAT.

Keying on *enabled* rather than *active* is the load-bearing part. A safety trip and the
boot window both clear `bActive` while the user's intent is unchanged; handing TSet back
to the heating curve in exactly those windows is the original conflict in a narrower
form.

An external `CS=` is refused while SAT owns the setpoint, with a visible `NG` and a
telnet line, rather than silently winning by being the last writer. SAT submits its own
`CS=` through the same shared queue, so a transient marker (`satCommandInFlight()`, set
around SAT's own enqueue and read synchronously by the OTDirect handler) distinguishes
the two without introducing a second command path.

## Decision Contract

### Must

- Exactly one predicate decides TSet ownership, and it is keyed on SAT being enabled.
- The OTDirect heating-curve / PI writer, the master-mode thermostat WRITE_DATA path and
  external `CS=` all consult that predicate.
- While SAT owns the setpoint, the CS heartbeat does not expire the MsgID 1 override.
  The heartbeat exists for an external actor that set a setpoint and vanished; SAT is not
  that actor, and it clears its own override in `satDisable()`.
- A refused external `CS=` is reported (`NG` plus a telnet line), never silently dropped.

### Must Not

- Do not gate a TSet writer on `state.sat.bActive`. That is the bug this ADR exists to
  close.
- Do not gate SAT's own `CS=` on ownership, or SAT refuses itself.

### Exceptions

- **The thermostat-timeout setback** (`OTDirect.ino:2470-2475`) may write MsgID 1 while
  SAT owns it. Maintainer decision, 2026-09-22: it is a fail-safe, and if the thermostat
  has gone quiet *and* SAT is not commanding, something must still keep the building from
  freezing. Strict single-writer would remove that net precisely when it is needed.

### Verification

- `src/OTGW-firmware/SATcontrol.ino` — `satOwnsControlSetpoint()` and
  `satCommandInFlight()`.
- `src/OTGW-firmware/OTDirect.ino` — the gated heating-curve, master-mode WRITE_DATA,
  `CS=` and CS-expiry sites.
- Field validation on an OTGW32: with SAT enabled and a safety trip forced, TSet must not
  revert to the heating curve or the thermostat.

## Alternatives Considered

- **Keep the `bActive` gate (status quo after TASK-761).** Rejected: it closes only the
  reported symptom. The four remaining writers stay open, and the gate itself drops out
  during a safety trip and at boot.
- **Strict single writer, setback included.** Rejected by the maintainer: it removes a
  fail-safe. A safety-tripped SAT sends nothing, so with the setback also blocked no one
  would command TSet at all and the boiler would fall back to its own behaviour.
- **Let an external `CS=` take over temporarily, until SAT's next write.** Rejected: the
  outcome depends on where you land in SAT's 0-to-300 s refresh window, which is a race
  dressed up as a feature. Least disruptive for existing automations, but unpredictable.
- **Treat an external `CS=` as "the user is taking over" and disable SAT.** Rejected as
  too large a side effect for one command, though it does have the virtue of being
  explicit.
- **Do nothing.** Rejected: five writers and no document is how the reported flip-flop
  happened, and the next one will look different.

## Consequences

**Positive:**

- One named predicate, so "who owns TSet" is answerable by reading one function.
- The safety-trip and boot windows no longer hand TSet back to the heating curve.
- An external `CS=` gets a visible refusal instead of an invisible race.
- SAT's setpoint survives its own quiet stretches: the cold cutoff and DHW-active windows
  no longer lose the override after 60 s.

**Negative:**

- **Breaking change for existing automations.** Anything that sets `CS=` over MQTT, REST,
  telnet or port 25238 while SAT is enabled now gets `NG`. That is deliberate — those
  writes were previously winning silently — but it will surface as "my automation stopped
  working". *Mitigation:* the refusal names the reason and the remedy (disable SAT); the
  behaviour is documented here and in the telnet output.
- **SAT is not an absolute owner.** The setback exception means a reader cannot assume
  "SAT enabled" implies "SAT is the only writer". *Mitigation:* the exception is stated in
  the Decision Contract and in the predicate's own comment.
- **The in-flight marker depends on a synchronous call path.** It works because
  `addCommandToQueue()` dispatches straight into `handleOTDirectCommand()` on OTDirect
  boards. A future async command path would break the distinction. *Mitigation:* stated
  at the definition site.
- **MsgID 0 remains unresolved.** Two masters still publish conflicting CH-enable bits in
  gateway mode. This ADR scopes to MsgID 1; the status-bit conflict needs its own
  decision.

## Open Questions

- [ ] MsgID 0 in gateway mode: the gateway emits its own status frames alongside the
      thermostat's, each with a different CH-enable bit, so SAT's `CH=0` is not reliably
      honoured. Should the gateway suppress its own MsgID 0 while a live thermostat is
      present, or should SAT's CH intent be applied to the relayed frame? Needs a decision
      and probably its own ADR.
- [ ] `other-projects/otgw-6.6/gateway.asm:1619-1630` suggests the PIC keeps a setpoint
      above 8 C alive past 60 s, which would make the expiry divergence OTDirect-specific.
      This is a single-source reading of assembly and is not verified on hardware; confirm
      before relying on it.

## Related Decisions

- **ADR-150 (COLD_SETPOINT gateway change)**: the cold cutoff is one of the windows where
  SAT holds a stable setpoint, which is what made the CS expiry bite.
- **ADR-162 (SAT force-boiler test hook)**: also scopes a deliberate exception to SAT
  control, and the same reasoning about visible, auth-gated overrides applies.

## References

- TASK-1150 (backlog); reported on Discord #dev-sat-mqtt 2026-05-29 by sergeantd.
- TASK-761 — the earlier, narrower fix this decision generalises.
- `src/OTGW-firmware/OTDirect.ino` — writers at 2080, 2473-2474, 2525-2526, 2631; expiry
  at 2033-2037; scheduler gating at 2015; status frame at 733.
- `src/OTGW-firmware/SATcontrol.ino` — `satDisable()` at 1853, refresh window at 312-345.
- `src/OTGW-firmware/OTGW-Core.ino:3550-3561` — the unarbitrated shared command queue.
