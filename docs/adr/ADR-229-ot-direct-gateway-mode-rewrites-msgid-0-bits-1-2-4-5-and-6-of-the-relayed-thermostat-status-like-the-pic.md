---
id: "ADR-229"
title: "OT-Direct gateway mode rewrites MsgID 0 bits 1, 2, 4, 5 and 6 of the relayed thermostat status like the PIC"
status: "Proposed"
date: "2026-11-11"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "otdirect"
  - "opentherm"
  - "gateway-mode"
  - "master-status"
  - "pic-parity"
aliases:
  - "MsgID 0 relay rewrite"
  - "HW SM BW CE H2 in gateway mode"
  - "readstatus parity"
components:
  - "OT-Direct relayed MsgID 0 master-status rewrite"
symbols:
  - "otdRelayedStatusFrame"
  - "otdEffectiveMasterFlags"
  - "applyOverrides"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-229 OT-Direct gateway mode rewrites MsgID 0 bits 1, 2, 4, 5 and 6 of the relayed thermostat status like the PIC

## Status

Proposed, 2026-11-11. Supersedes ADR-208 when accepted (ADR-208's bit 0 rule is carried over unchanged, below). The supersession is recorded on both ADRs with `adr supersede` at acceptance; ADR-208 stays as it is until then.

## Status History

```yaml
status_history:
  - date: 2026-11-11
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal (TASK-1805; User Decision 'Like the PIC: new ADR', relayed by the coordinator session)
    changed_via: adr-kit
```

## Context

In gateway mode the gateway sits between a thermostat and the boiler. MsgID 0 (Status) is a READ_DATA whose high byte is the master status: bit 0 CH enable, bit 1 DHW enable, bit 2 cooling enable, bit 4 CH2 enable, bit 5 summer mode, bit 6 DHW blocking (OpenTherm 2.2). The serial commands HW=, CE=, H2=, SM= and BW= exist to change those bits.

- **What the PIC does.** For the thermostat's own MsgID 0 the PIC replaces bits of the master status before the frame goes to the boiler (`readstatus`, `other-projects/otgw-6.6/gateway.asm:2712-2742`). Per bit, a mask says whether the gateway replaces the bit and a value says with what:
  - bit 0: mask when a control setpoint is held (CS=, `SysCtrlSetpoint` = bit 7 of `controlsetpt1` clear, `:342`) or there is heat demand (`HeatDemand`, `:452`), value `!CHModeOff` (CH=). This is the rule ADR-208 brought to OT-Direct.
  - bit 1: mask when `HotWaterSwitch` (HW=0 or HW=1) or a DHW push is running (`dhwpushspoof`), value `HotWaterEnable` (HW=1) or the push (`SetHotWater` `:4348-4367`, flags `:488`, `:517-518`).
  - bit 2: mask when a cooling level is held (CL=, `SysCoolLevel` = bit 7 of `coolinglevel1` clear, `:348`, `SetCoolLevel` `:4411-4421`), value `!CoolModeOff` (CE=, `SetCoolEnable` `:4403-4409`).
  - bit 4: mask when a CH2 control setpoint is held (C2=, `SysCH2Setpoint` = bit 7 of `controlsetpt3` clear, `:345`, `SetCtrlSetpt2` `:5229`), value `!CH2ModeOff` (H2=, `SetCentralHeat2` `:5221-5227`).
  - bit 5: mask and value 1 when `SummerMode` (SM=1, `:516`, `:4522-4524`); SM=0 leaves the thermostat's bit.
  - bit 6: mask and value 1 when `DHWBlocking` (BW=1, `:465`, `:4530-4532`); BW=0 leaves the thermostat's bit.
  Bits 3 and 7 always pass. The reply to the thermostat gets its own master status back (`statusreturn`, `:2745-2750`).
- **What OT-Direct does today.** HW=, H2=, SM=, BW= and CE= only change `otMasterStatusFlags` (`src/OTGW-firmware/OTDirect.ino:3117-3211`). Only `buildStatusRequest()` (`:869`) reads it, for the gateway's own MsgID 0. Since TASK-1701 the gateway sends no frames of its own while a thermostat is connected in gateway mode (`otdOwnFramesAllowed()`, `:864`; ADR-224 relies on this), so that frame is not sent there. The relayed thermostat MsgID 0 gets only ADR-208's bit 0 rule (`otdRelayedStatusFrame()`, `:840-847`, called from `applyOverrides()`, `:616-630`). The five commands are acknowledged ("HW: 0") but change nothing on the bus. Example: a Home Assistant automation sends HW=0 to block hot water; the boiler keeps seeing the thermostat's DHW enable.
- **Why ADR-208 has to go.** ADR-208 says "Must not rewrite any MsgID 0 bit other than bit 0" and names the other bits a known limitation, because the gateway's own MsgID 0 schedule still carried them. TASK-1701 removed that schedule with a thermostat present, so the premise no longer holds (audit TASK-1795, `docs/reviews/2026-11-11_otdirect-pic-parity-audit/`).

User Decision (Robert, maintainer, 2026-11-11, AskUserQuestion in the coordinator session, relayed): "Like the PIC: new ADR".

## Decision

In OT-Direct gateway mode the relayed thermostat READ_DATA MsgID 0 gets bits 0, 1, 2, 4, 5 and 6 of its master status by the PIC `readstatus` rule (`gateway.asm:2712-2742`), each bit from the OT-Direct state of the command that owns it; bits 3 and 7 and the low byte pass unchanged, the parity is recomputed when the frame changed, and the thermostat gets its own master status back in the reply.

The mapping to OT-Direct state:

| Bit | Replaced when (mask) | With (value) | OT-Direct state |
|---|---|---|---|
| 0 | a CS override is active | `!otChModeOff` | as ADR-208: `otdEffectiveMasterFlags()`, `otdCsOverrideActive()` |
| 1 | HW=0 or HW=1, or a DHW push runs | HW=1, or the push | `otDHWOverride` (0xFF = auto), `otDHWPushState` |
| 2 | a CL= cooling level is held | CE= | the MsgID 7 write override, `otCoolingEnable` |
| 4 | a C2= CH2 setpoint is held | H2= | the MsgID 8 write override, H2= state |
| 5 | SM=1 | 1 | `otSummerMode` |
| 6 | BW=1 | 1 | `otDHWBlocking` |

Bit 0 keeps OT-Direct's ADR-208 trigger (any active CS override) rather than the PIC's `SysCtrlSetpoint`/`HeatDemand` pair: that was Robert's decision for ADR-208 (entry 264) and is not reopened here.

## Decision Contract

### Must

- In gateway mode, a relayed thermostat READ_DATA MsgID 0 has its high byte computed by one pure function from the frame's high byte and the command state in the table above; `otdRelayedStatusFrame()` applies it and recomputes the parity when the frame changed.
- The reply to the thermostat for a rewritten MsgID 0 keeps the boiler's low byte and the thermostat's original high byte (`otdStatusReplyData()`, unchanged from ADR-208).
- With no command state active (HW=A, no CL=, no C2=, SM=0, BW=0, no CS override) the frame passes byte for byte.

### Must Not

- Must not touch bits 3 and 7 of the master status or the low byte.
- Must not rewrite in monitor or bypass mode, or any MsgID 0 that is not a READ_DATA.
- Must not replace bit 2 or bit 4 without the held CL= or C2= value (CE= or H2= alone changes nothing, as on the PIC).

### Exceptions

- Bit 0 trigger: the ADR-208 predicate (any active CS override), see Decision.

### Verification

- A host test compiles the real rewrite block cut from `src/OTGW-firmware/OTDirect.ino` (the cut-between-markers pattern of `tests/test_otd_status_relay.py`) with one case per bit (mask on and off), the pass-through case and the reply. It fails on the code before this ADR.

## Alternatives Considered

- **Keep ADR-208 (bit 0 only).** Rejected: HW=, CE=, H2=, SM= and BW= are acknowledged but have no effect with a thermostat in gateway mode, which is the gap the audit found; users and Home Assistant automations get a false success.
- **Send the gateway's own MsgID 0 again with a thermostat present.** Rejected: TASK-1701 removed it because two masters with different status bits made the boiler follow whichever came last (tjfs, 2026-11-07).
- **Force all five bits from `otMasterStatusFlags` (always replace).** Rejected: unlike the PIC it would override the thermostat's DHW, summer and CH2 bits even when no command was given (the board defaults), so a thermostat in its own summer mode or with DHW off would be overruled. The PIC replaces a bit only while its command is in force.

## Consequences

**Positive:**

- HW=, CE= (with CL=), H2= (with C2=), SM= and BW= work in OT-Direct gateway mode with a thermostat, as on a PIC board; Home Assistant and MQTT controls of these bits stop being no-ops.
- Without commands nothing changes: the thermostat's status reaches the boiler as it is.

**Negative:**

- New relay semantics for five bits: after HW=0 the boiler no longer sees the thermostat's DHW request. That is the intent and the PIC behaviour; a user who forgot an old HW=0 notices it. Mitigation: HW=A, SM=0, BW=0, CL=0 and C2=0 hand the bits back.
- SM= is persisted (`settings.otd.bSummerMode`), so SM=1 keeps forcing bit 5 after a reboot, as the PIC's `SummerMode` in its config (`conf,3`).
- The decision rests on the PIC source and code reading; a capture from an OT-Direct user with a thermostat in gateway mode is still the field proof (open, as for ADR-208).

## Open Questions

- [ ] Acceptance by Robert (maintainer); this ADR is never self-accepted.

## Related Decisions

- **ADR-208** (superseded when accepted): bit 0 rule, carried over unchanged.
- **ADR-224**: relies on the gateway sending no own frames with a thermostat present (TASK-1701), the premise this ADR builds on.
- **ADR-223**: TT=/TC= answer-state-only parity with the PIC; same "like the PIC" direction for OT-Direct gateway mode.

## References

- `other-projects/otgw-6.6/gateway.asm:2712-2750` (`readstatus`, `statusreturn`), flags `:342-348`, `:461-465`, `:488`, `:516-518`, handlers `:4348-4367`, `:4403-4421`, `:4522-4532`, `:5221-5233`.
- `src/OTGW-firmware/OTDirect.ino:616-630` (`applyOverrides`), `:840-847` (`otdRelayedStatusFrame`), `:864` (`otdOwnFramesAllowed`), `:869` (`buildStatusRequest`), `:2989-3006` (C2=), `:3117-3226` (HW=, H2=, SM=, BW=, CE=, CL=).
- TASK-1805; audit TASK-1795 (`docs/reviews/2026-11-11_otdirect-pic-parity-audit/`, report A, A2).
