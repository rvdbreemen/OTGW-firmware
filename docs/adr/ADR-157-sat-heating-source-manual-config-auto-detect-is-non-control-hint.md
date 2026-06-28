# ADR-157: SAT Heating-Source Selection Is Manual Configuration; Auto-Detect Is a Non-Control Hint

## Status

Proposed. Date: 2026-06-28.

**Decision Maker:** User: Robert (maintainer), prompted by George (SAT author) asking on #dev-sat-mqtt for a reliable way to detect gas boiler vs heat pump vs hybrid.

## Status History

status_history:
  - date: 2026-06-28
    status: Proposed
    changed_by: Agent
    reason: Capture the spec-grounded decision that heating-source is manual config, not auto-detection (TASK-943).
    changed_via: adr-kit

## Context

The SAT (Smart Autotune Thermostat) control loop diverges by *heating source*: a heat pump is driven differently from a gas boiler (max-setpoint capped at ~40C, different cycle timing, MM=100% modulation). The source therefore feeds real boiler commands, not just telemetry — see `satGetEffectiveHeatingSource()` consumers at `SATcontrol.ino:228` (max-setpoint cap), `:253`, `:266` (cycle seconds), `:274`, `:293`.

TASK-891.8 introduced a `satsource` setting with an AUTO default that resolved to an auto-detected source (`state.sat.iDetectedHeatingSource`). The detection had two faults discovered during the 2026-06-28 beta-readiness review (TASK-943):

1. **Mis-wired.** Detection was set inside `print_vh_configmemberid` (`OTGW-Core.ino`), which decodes OT **MsgID 74 (ventilation/heat-recovery config)** and keyed off ventilation bit0 — not the intended MsgID 3 boiler config. Because MsgID 74 is absent on boiler-only and most heat-pump installs, the detected value was effectively stuck at `GAS_BOILER` (inert).

2. **Inherently unreliable even when wired correctly.** OpenTherm has no device-class / "I am a heat pump" field anywhere on the wire. The spec frames the protocol as serving boilers, heat pumps, and ventilation systems without distinguishing them (spec v4.2:458). MsgID 3 (Slave Configuration, HB) carries only: DHW-present (bit0), control-type (bit1), **cooling-config (bit2)**, DHW-storage (bit3), pump-control (bit4), CH2 (bit5), remote-fill (bit6), heat/cool-owner (bit7) — see spec v4.2:1603-1620. The only usable proxy for "heat pump" is bit2 "cooling supported", and it is lossy.

George (SAT author) asked for a *reliable* detection method. The evidence says reliability is unattainable from OT alone.

## Decision

Heating-source selection is **manual configuration** (`settings.sat.iHeatingSource` = `GAS_BOILER` / `HEAT_PUMP` / `HYBRID`), authoritative for control. The AUTO default resolves to a safe gas-boiler control profile. Auto-detect is demoted to a **non-control telemetry hint only** (`state.sat.iDetectedHeatingSource`, published as `heating_source_detected`), read from the correct MsgID 3 HB bit2 in `print_slavememberid`.

In code (TASK-943):

- `satGetEffectiveHeatingSource()` (`SATcontrol.ino:204`): `AUTO` returns `SAT_SRC_GAS_BOILER`; any manual value is returned verbatim.
- The detect read moved out of the MsgID 74 handler into `print_slavememberid` (`OTGW-Core.ino`), reading `OTdata.valueHB & 0x04` (cooling supported), and is consumed by nothing in the control path.

## Alternatives Considered

### Alternative A: Rewire auto-detect to MsgID 3 bit2 and keep AUTO driving control

Fix only the mis-wiring (bit), leave AUTO consuming the detected source for control. **Rejected.** This would *activate* a dormant, lossy control input on every cooling-capable system. A heating-only heat pump (common in NL retrofit) never sets bit2, so it would be mis-driven as a gas boiler; cooling-capable gas-fired absorption units would be mis-driven as heat pumps. Turning a dead-but-safe input into a live-but-wrong one is a regression, not a fix.

### Alternative B: Manual config authoritative; auto-detect is a non-control hint (chosen)

Manual selection drives control; the cooling bit only informs a UI hint so the user knows what the bus advertises. Correct by construction (a human declares the source), honest about the spec limitation, and KISS. Cost: a heat-pump user on AUTO must set `HEAT_PUMP` once.

### Alternative C: Out-of-band source detection (Modbus / second OTGW)

Detect source via a side channel — Modbus heat-pump registers (TASK-641) or a slave-OTGW-to-master notification for hybrid (TASK-892). **Rejected for now** (deferred, not discarded): real but experimental, hardware-specific, out of beta scope. It is the only path to *true* hybrid awareness and remains the future direction; it does not change today's OT-only conclusion.

### Alternative D: Do nothing (leave AUTO driving the mis-wired detect)

**Rejected.** Leaves a control input keyed off a ventilation bit, with a code/comment contradiction (`SATtypes.h` said "MsgID 3" while the code read MsgID 74). Latent and misleading.

## Consequences

**Benefits**

- Control correctness no longer depends on an unreliable wire signal; the source is whatever the user declares.
- Behaviour-neutral for the common case: MsgID 74 was absent on boiler-only *and* heat-pump installs, so AUTO already behaved as gas boiler — this removes a dead, lossy input rather than a working one.
- Answers George's question durably: reliable source auto-detection is impossible from OT alone (spec v4.2:458, 1603-1620), so manual config is the design, not a stopgap.
- The cooling-bit hint is still surfaced (`heating_source_detected`) so a user can see "the bus says cooling-capable" and choose accordingly.

**Trade-offs**

- A heat-pump or hybrid user on the AUTO default must manually select their source once. Mitigated by the visible `heating_source_detected` hint nudging them.
- "AUTO" is now a slight misnomer (it does not auto-configure the source for control). Documented in the enum comment (`SATtypes.h`).

**Risks and mitigations**

- *Risk*: an existing alpha tester who (by luck of MsgID 74) was being driven as a heat pump regresses to gas-boiler timing on AUTO. *Mitigation*: that path was never validated or trustworthy ("exact detect bit pending Elga validation" per the old comment); the safe gas-boiler profile is the conservative default, and manual `HEAT_PUMP` restores heat-pump timing deterministically.
- *Risk*: hybrid systems remain undetectable. *Mitigation*: accepted and documented; true hybrid awareness is tracked in TASK-892 (out-of-band channel), not solvable on one OT bus.

## Related Decisions

- **ADR-150 (SAT Per-Heating-System COLD_SETPOINT)**: references the TASK-891.8 heating-source/heating-system enum split as context. ADR-157 narrows the *source* axis: manual selection is authoritative; auto-detect does not drive control. No conflict — ADR-150's COLD_SETPOINT is keyed on the heating *system* (radiators vs underfloor), which OT also cannot detect and which likewise defaults safely.
- **ADR-156 (Unified heat/cool/off HA climate)**: complements; both concern how OT bus state maps to user-facing semantics.

## References

- TASK-943 (this decision); TASK-891.8 (the superseded auto-detect-drives-control behaviour, which had no dedicated ADR); TASK-892 (hybrid coordination, deferred); TASK-641 (Modbus heat-pump side-channel, deferred).
- Code: `SATcontrol.ino:204-209` (`satGetEffectiveHeatingSource`), `:228/253/266/274/293` (control consumers); `OTGW-Core.ino` `print_slavememberid` (MsgID 3 HB bit2 hint read), `print_vh_configmemberid` (MsgID 74 detect removed); `SATtypes.h:47-52` (enum), `:214` (hint field).
- Spec: OpenTherm Protocol Specification v4.2 — line 458 (no device-class distinction), lines 1603-1620 (MsgID 3 Slave Configuration flag8 bit definitions, cooling-config = bit2).
- Field evidence: George, #dev-sat-mqtt 2026-06-20, on hybrid invisibility ("You can't see it burn thru the OTGW from the Elga"), captured in TASK-892.

## Enforcement

No automated gate. This is a guideline-level ADR (per ADR-080): the decision is a design principle (manual source is authoritative; the cooling bit is a hint), enforced by code review rather than a pattern matcher. The load-bearing invariant — that `state.sat.iDetectedHeatingSource` is never read by the control path — is small enough to verify by grepping its readers at review time.
