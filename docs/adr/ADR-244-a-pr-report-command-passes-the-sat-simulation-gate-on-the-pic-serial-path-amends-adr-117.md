---
id: "ADR-244"
title: "A PR= report command passes the SAT simulation gate on the PIC serial path (amends ADR-117)"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "SAT simulation"
  - "bus isolation"
  - "PIC serial"
  - "PR= report command"
  - "PIC settings"
aliases:
  - "satSimulationBlocksBusTx"
  - "simulation gate"
  - "pic-serial"
  - "PR=A"
components:
  - "sendPICSerial"
symbols:
  - "sendPICSerial"
  - "satSimulationBlocksBusTx"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-244 A PR= report command passes the SAT simulation gate on the PIC serial path (amends ADR-117)

## Status

Accepted, 2026-11-15. Amends ADR-117 decision 1 (bus isolation) for path (a) only; ADR-117 stays Accepted and
unchanged otherwise. Guideline-level per ADR-080: verified by the host test named below, no evaluate.py gate.

Decision Maker: Robert van den Breemen (maintainer), Discord DM 2026-11-15, relayed by the coordinator: "A"
(message 1571450830035484673) on the TASK-1943 question 1571450615744168028 (A = let PR= through during the
simulation, CS= and the other commands stay blocked; B = keep the block and only show that the PIC panel is not
available during the simulation).

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer chose A for TASK-1943 (Discord DM 1571450830035484673 on question 1571450615744168028)
    changed_via: adr-kit lifecycle
```

## Context

ADR-117 decision 1 keeps every boiler-side byte on the device while a simulation runs: the gate helper
`satSimulationBlocksBusTx()` (`src/OTGW-firmware/SATcontrol.ino`) returns true while the SAT boiler simulation
(`settings.sat.bSimulation`) or the older serial-replay simulation (`state.debug.bOTGWSimulation`) is on, and
`sendPICSerial()` (`src/OTGW-firmware/OTGW-Core.ino`, path (a)) then drops the command. That holds for every
command sent to the PIC (the gateway's PIC16F1847 or PIC16F88 microcontroller), the report command `PR=`
included.

`PR=<letter>` puts nothing on the OpenTherm bus and changes no PIC state: in the PIC firmware 6.8 the serial
parser sends `PR` to `ReportSetting` (`other-projects/otgw-6.8/gateway.asm` line 3810), which jumps to
`PrintSetting` (line 4809), and `PrintSetting` checks the length and the letter and dispatches through
`PrintTable` (lines 2154 to 2164) to a routine that prints the setting. Source: the 6.8 assembly only; the 6.x
manual describes `PR=` the same way (print selected information, line 102 of the same file).

Evidence (TASK-1943): on `.63` (Classic, PIC 6.8, no boiler, SAT simulation on) `/api/v2/pic/settings` stayed
empty, the gateway mode stayed unknown and the return-water poll `PR=E` gave nothing; telnet showed the queue
sending each `PR=` and no `PR:` reply. 09's bench, red/green on `.63`: simulation on, no `PR=` reaches the PIC and
0 of 15 settings fill; simulation off, the PIC answers in 15 to 50 ms and the settings fill. Only rigs without a
boiler are affected: a real boiler frame turns the simulation off (ADR-117 decision 2).

## Decision

On the PIC serial path, a command that starts with `PR=` is sent to the PIC also while a simulation is on; every
other command is held back by `satSimulationBlocksBusTx()` exactly as ADR-117 decides.

## Decision Contract

### Must

- `sendPICSerial()` sends a command that starts with `PR=` without calling the simulation gate.
- Every other command (for example `CS=`, `GW=`, `SW=`) still goes through `satSimulationBlocksBusTx()` and is
  dropped while a simulation is on, with the trace and the last-blocked command as before.

### Must Not

- Must not exempt anything on the OT-Direct master path (`sendMasterRequestAsync()`, ADR-117 paths (b) and (c)):
  that path sends real OpenTherm frames.
- Must not exempt other PIC commands: commands such as `PS=` change what the PIC prints or its state and are
  outside this decision.

### Exceptions

- None.

### Verification

- `test/host/test_pic_sim_gate_pr.py`: compiles the real gate block of `sendPICSerial()` and the real
  `satSimulationBlocksBusTx()`, sliced by anchor. With the SAT simulation on, `PR=A` and `PR=E` pass and leave no
  blocked-command trace, `CS=40` and `GW=1` are held back and traced; with the serial-replay simulation on,
  `PR=M` passes and `CS=40` is held; with no simulation every command passes. Red on the code before this ADR (the
  `PR=` cases are held), green on it.
- Bench: on `.63` with the SAT simulation on, the settings cycle fills `/api/v2/pic/settings` (TASK-1943 AC3).

## Alternatives Considered

- **Keep the block and show "not available during simulation" in the PIC panel (option B).** Rejected by the
  maintainer: the bench rig then cannot show PIC settings, the gateway mode or the `PR=E` return-water reading at
  all, although reading them cannot reach the boiler.
- **Exempt every read-only PIC command.** Rejected: "read-only" is not a property the firmware knows per
  command, and commands such as `PS=1` change what the PIC prints; one prefix with a traced source keeps the
  isolation easy to check.
- **Turn the simulation off while the settings cycle runs.** Rejected: it would let the queued simulation
  commands out in that window, which is what ADR-117 prevents.

## Consequences

**Positive:**

- A boiler-less bench with the SAT simulation on shows the PIC settings, the gateway mode and the `PR=E` reading
  again (0 of 15 fields before, all 15 on 09's red/green with the simulation off).
- The cut stays boiler-side: no OpenTherm frame and no PIC setting change leaves the device during a simulation.

**Negative:**

- During the serial-replay simulation the real PIC's `PR:` replies arrive next to the replayed lines. Mitigation:
  the replies are text lines that `handlePRresponse()` handles by register, the same as on a live gateway. Not
  checked: whether a replay file that contains `PR:` lines of its own then shows two answers for one register.
- `PR=` commands no longer show in the simulation trace ring. Mitigation: they are visible in the queue log
  (`CmdQueue: ... cmd[PR=x]`) and their replies in the telnet log.

## Open Questions

- [x] Maintainer choice for TASK-1943: A or B. Answered A on 2026-11-15 (see Status).

## Related Decisions

- **ADR-117 (SAT Simulation Contract)**: amended by this ADR for path (a) and the `PR=` prefix only; ADR-117 stays
  Accepted and the rest of it stands unchanged.
- **ADR-058 (Non-blocking PIC Command/Response for PR= Queries)**: the `PR=` replies this ADR lets through are
  handled by that asynchronous path.

## References

- TASK-1943 (backlog), notes of 2026-11-15 (trace, 09's red/green bench on `.63`, decision).
- `src/OTGW-firmware/OTGW-Core.ino`, `sendPICSerial()`, the path (a) gate block.
- `src/OTGW-firmware/SATcontrol.ino`, `satSimulationBlocksBusTx()`.
- `other-projects/otgw-6.8/gateway.asm`: `ReportSetting` (line 3810 dispatch, line 4809), `PrintSetting` and
  `PrintTable` (lines 2154 to 2164), the `PR=` description (line 102).
