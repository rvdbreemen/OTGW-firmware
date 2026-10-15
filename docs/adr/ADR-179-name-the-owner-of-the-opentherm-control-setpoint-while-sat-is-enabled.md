---
id: "ADR-179"
title: "Name the owner of the OpenTherm control setpoint while SAT is enabled"
status: "Accepted"
date: "2026-10-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
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
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-179 Name the owner of the OpenTherm control setpoint while SAT is enabled

## Status

Accepted, 2026-10-15.

## Status History

```yaml
status_history:
  - date: 2026-09-22
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-15
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted 2026-10-15 after the code-vs-ADR verification; open questions answered in place (TASK-1237)
    changed_via: adr-kit lifecycle
```

## Context

MsgID 1 (TSet, the boiler control setpoint) is the single most consequential value the
firmware writes. Until now no document said who owns it, and five code paths could write
it independently.

Reported on Discord (#dev-sat-mqtt, 2026-05-29) by sergeantd: *"When SAT is enabled,
ONLY SAT should drive TSet. Otherwise a conflict will occur."* The visible symptom was a
45 <-> 10 flip-flop every 60 seconds. TASK-761 fixed that same day by gating OTDirect's
heating-curve writer on `state.sat.bActive` (the TASK-183 PI / heating-curve block in
`OTDirect.ino`, now at about 2181-2203). The symptom went away; the rule did not exist.

Line numbers in this section are refreshed to the `dev` branch head on 2026-10-15 and will drift; the
symbol names are the stable reference.

A verification pass over dev HEAD (2026-09-22), including an adversarial completeness
check that grepped every `setOverride(1` / `updateWriteCache(1` / `enqueueWriteCommand(1`
site, found the writer set is exactly five and that four remained ungated. In order of
how easily they fire:

1. **Thermostat-timeout setback** (the fail-safe setback in `OTDirect.ino`, about
   2586-2595, `setOverride(1, ...)` / `updateWriteCache(1, ...)`). `settings.otd.bFailSafe`
   defaults true and `iSetbackTimeout` defaults 30 s (`OTDirecttypes.h:116,113`), and
   gateway is the default mode. A quiet thermostat hands TSet to a 16 C setback with no
   external actor involved.
2. **`CS=` has no gate** (the `CS=` branch of `handleOTDirectCommand()` in
   `OTDirect.ino`, now about 2742-2775), reachable from MQTT (Message Queuing Telemetry Transport) `ctrlsetpt` and the raw
   `command` topic, `POST /api/v2/otgw/commands`, the port-25238 bridge and telnet.
   `addCommandToQueue()` (`OTGW-Core.ino`, about 3616) arbitrates nothing, so the last
   writer wins.
3. **Refresh versus expiry.** SAT re-sends CS on change or every `SAT_CMD_REFRESH_MS`
   (300 s, `SATcontrol.ino`, about 317); OTDirect expires the override after
   `OT_CSC2_EXPIRY_MS` (60 s, `OTDirect.ino`, about 456; the expiry check is at about
   2148). `otCSLastCommandMs` is never stamped by the scheduler, so the expiry really
   fires while SAT is deliberately holding a stable setpoint: during the ADR-150 cold
   cutoff and while DHW (domestic hot water) is active.
4. **Silent handover on a safety trip.** `satDisable()` (`SATcontrol.ino`, about 1916)
   clears `state.sat.bActive` and leaves `settings.sat.bEnabled` true, so a gate keyed
   on `bActive` re-arms the heating curve while the user still believes SAT is in
   charge.
5. **Master-mode thermostat WRITE_DATA** (`OTDirect.ino`, about 2646), ungated.

One question had to be answered before any fix, because it decides whether MsgID 1
arbitration is even sufficient: does SAT's `CH=` reach the boiler on pass-through
thermostat frames? It does not, and the real answer is worse. `otMasterStatusFlags` is
read at exactly one bus-facing site, `buildStatusRequest()` (`OTDirect.ino`, about
756), and there is no `setOverride(0, ...)` anywhere, so a thermostat's own MsgID 0 is
relayed unmodified. But `scheduleMasterRequest()` runs in gateway mode too: only monitor
mode is excluded (the `timerOTSchedule` / `IS_MONITOR_MODE()` gate in `OTDirect.ino`,
about 2121), so the boiler receives the thermostat's MsgID 0 and
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

**Scope: OT-Direct boards.** This decision, and the refusal of an external `CS=` in
particular, applies where OTDirect drives the bus: the `esp32` target (OTGW32) and the
`esp32-combo` image when it boots in OT-Direct mode. On boards with a PIC (the gateway's OpenTherm microcontroller), that is
`esp32-classic` and `esp32-combo` in PIC mode, `addCommandToQueue()` (`OTGW-Core.ino`, about 3616-3623)
reaches `handleOTDirectCommand()` only when no PIC is enabled, so there an external `CS=`
goes to the PIC and is not refused. Port 25238 cannot be gated on those boards either,
because the PIC byte stream is kept verbatim in both directions (ADR-177). How SAT keeps
its setpoint on PIC boards is follow-up work in TASK-1236.

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

- **The thermostat-timeout setback** (fail-safe setback in `OTDirect.ino`, about
  2586-2595) may write MsgID 1 while
  SAT owns it. Maintainer decision, 2026-09-22: it is a fail-safe, and if the thermostat
  has gone quiet *and* SAT is not commanding, something must still keep the building from
  freezing. Strict single-writer would remove that net precisely when it is needed.
- **PIC boards are out of scope** (see Scope under Decision). On a PIC board an external
  `CS=` reaches the PIC and is not refused; that gap is tracked in TASK-1236.

### Verification

- `src/OTGW-firmware/SATcontrol.ino` — `satOwnsControlSetpoint()`, `satCommandInFlight()`
  and `satEnqueueOwnCS()`, the single path every SAT `CS=` takes, the `CS=0` releases in
  `satDisable()` and the boot-safety path included. A SAT `CS=` sent any other way is
  refused as external while SAT is enabled (found on the bench 2026-09-23, alpha.371).
- `src/OTGW-firmware/OTDirect.ino` — the gated heating-curve, master-mode WRITE_DATA,
  `CS=` and CS-expiry sites.
- Field validation on an OTGW32: with SAT enabled and a safety trip forced, TSet must not
  revert to the heating curve. (Handing TSet back to the thermostat is deliberate: the
  trip calls `satDisable()`, which sends `CS=0` and releases the override.)

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
  telnet or port 25238 while SAT is enabled now gets `NG` on an OT-Direct board. That is
  deliberate (those writes were previously winning silently), but it will surface as
  "my automation stopped working". *Mitigation:* the refusal names the reason and the remedy (disable SAT); the
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
  decision, tracked in TASK-1238.
- **PIC boards are not covered.** On `esp32-classic` and `esp32-combo` in PIC mode an
  external `CS=` is not refused, and the PIC expires a `CS` setpoint of 8 C or more that
  has not been refreshed within its one-minute heartbeat window (see the answered Open
  Question below). SAT re-sends an unchanged setpoint only every `SAT_CMD_REFRESH_MS`
  (300 s, `SATcontrol.ino`, about 317), so on PIC boards SAT's setpoint likely lapses
  between refreshes. This is a single-source reading of the PIC assembly and has not
  been reproduced on the bench; it is tracked in TASK-1236.

## Open Questions

- [x] MsgID 0 in gateway mode: the gateway emits its own status frames alongside the
      thermostat's, each with a different CH-enable bit, so SAT's `CH=0` is not reliably
      honoured. Should the gateway suppress its own MsgID 0 while a live thermostat is
      present, or should SAT's CH intent be applied to the relayed frame? Needs a decision
      and probably its own ADR.
  **Answered 2026-10-15 by User: Robert van den Breemen:** moved out of scope of this
  ADR, which decides MsgID 1 only. The MsgID 0 decision is tracked in TASK-1238 and will
  get its own ADR if one is needed.
- [x] `other-projects/otgw-6.6/gateway.asm:1619-1630` suggests the PIC keeps a setpoint
      above 8 C alive past 60 s, which would make the expiry divergence OTDirect-specific.
      This is a single-source reading of assembly and is not verified on hardware; confirm
      before relying on it.
  **Answered 2026-10-15 by User: Robert van den Breemen:** the premise is inverted.
  `CommandExpiry` (`gateway.asm:1619-1630`) loads `controlsetpt1`, masks it with
  `0xF8`, and then: if `HeartbeatCS` is set (a `CS` arrived in the past minute) it keeps
  the setpoint; otherwise it keeps it only when the masked value is zero (setpoint below
  8 C), and for any setpoint of 8 C or more it sets `SysCtrlSetpoint` (line 1629),
  dropping the override. So the PIC also expires `CS` after about a minute, which
  matches the `OT_CSC2_EXPIRY_MS` comment in `OTDirect.ino` ("matches PIC heartbeat
  window", about 456); the expiry is not OTDirect-specific. Consequence: SAT refreshes
  an unchanged setpoint only every `SAT_CMD_REFRESH_MS` (300 s), so on PIC boards SAT's
  setpoint likely lapses between refreshes. Still a single-source reading of assembly,
  not yet reproduced on the bench; tracked in TASK-1236 and recorded under
  Consequences.

## Related Decisions

- **ADR-150 (COLD_SETPOINT gateway change)**: the cold cutoff is one of the windows where
  SAT holds a stable setpoint, which is what made the CS expiry bite.
- **ADR-162 (SAT force-boiler test hook)**: also scopes a deliberate exception to SAT
  control, and the same reasoning about visible, auth-gated overrides applies.

## References

- TASK-1150 (backlog); reported on Discord #dev-sat-mqtt 2026-05-29 by sergeantd.
- TASK-761: the earlier, narrower fix this decision generalises.
- TASK-1236: SAT on PIC boards, the setpoint lapsing between 300 s refreshes.
- TASK-1238: how SAT's CH-enable reaches the boiler in OT-Direct gateway mode (MsgID 0).
- `src/OTGW-firmware/OTDirect.ino` (line numbers as of 2026-10-15, symbols are the
  stable reference): MsgID 1 writers in the heating-curve block (about 2181-2203), the
  fail-safe setback (about 2586-2595), the master-mode WRITE_DATA path (about 2646) and
  the `CS=` branch of `handleOTDirectCommand()` (about 2742-2775); CS expiry check at
  about 2148 (`OT_CSC2_EXPIRY_MS` at 456); scheduler gating at about 2121;
  `buildStatusRequest()` at about 756.
- `src/OTGW-firmware/SATcontrol.ino`: `satOwnsControlSetpoint()` (about 1906-1913),
  `satDisable()` (about 1916), `SAT_CMD_REFRESH_MS` (about 317) and the refresh helpers
  after it.
- `src/OTGW-firmware/OTGW-Core.ino`: `addCommandToQueue()` (about 3616-3623), the
  unarbitrated shared command queue and its PIC / OT-Direct dispatch.
- `other-projects/otgw-6.6/gateway.asm:1619-1630`: the PIC's `CommandExpiry` routine.
