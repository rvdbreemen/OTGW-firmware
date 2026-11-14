---
id: "ADR-241"
title: "OT-Direct master mode relays the thermostat's writes to the boiler in the next free master slot and returns the boiler's answer (amends ADR-066)"
status: "Proposed"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-179"
topics:
  - "otdirect"
  - "master-mode"
  - "opentherm"
  - "pic-parity"
  - "thermostat"
aliases:
  - "master-mode write relay"
  - "virtual boiler writes"
  - "TASK-1812"
  - "forward thermostat WRITE_DATA"
components:
  - "OT-Direct master-mode thermostat write relay"
symbols:
  - "handleMasterModeSlaveFrame"
  - "replyToThermostat"
  - "scheduleMasterRequest"
  - "OTD_REPLY_DEADLINE_MS"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-241 OT-Direct master mode relays the thermostat's writes to the boiler in the next free master slot and returns the boiler's answer (amends ADR-066)

## Status

Proposed, 2026-11-15. Guideline-level per ADR-080: verified by host tests, no evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-15
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Related to ADR-179
    changed_via: adr-kit lifecycle
```

## Context

OT-Direct is the gateway firmware path without a PIC: the ESP32-S3 drives the OpenTherm boiler (master port) and the
thermostat (slave port) itself. In master mode the gateway runs its own polling schedule to the boiler. When a
thermostat is still on the slave port (`bEnableSlave`), ADR-066 lets the gateway answer it as a "virtual boiler" from
the boiler value cache, with "instant responses" listed as a benefit.

ADR-066 does not cover the thermostat's writes. Today `handleMasterModeSlaveFrame()` (`OTDirect.ino`) answers every
thermostat WRITE_DATA with a local WRITE_ACK echo of the thermostat's own data, and acts only on MsgID 1, 16 and 24.
A write of the DHW setpoint (MsgID 56), the max CH setpoint (57), the max modulation (14) or any other ID is
acknowledged and dropped: the thermostat believes the boiler has the value, the boiler never saw it (audit TASK-1795,
report E, U1). The PIC relays a thermostat frame and returns the boiler's reply (WRITE_ACK, Data-Invalid or
Unknown-DataID). Robert decided on 2026-11-11: "Forward to the boiler" (TASK-1812).

Timing constraint, OpenTherm spec v4.2 (`docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`):
the slave answers 20 ms to 400 ms after the end of the master frame, typically within 100 ms (:1093-1095); "In case
the slave is functioning as a gateway ... it must wait for an answer from the next slave in line, so the maximum
waiting time does not apply" (:1097-1098); the master waits 100 ms (MWT) between conversations (:1100); an unanswered
conversation is retried at the next scheduled time (4.5, :1259-1266). In master mode the master port can be busy with
one of the gateway's own frames when a thermostat write arrives.

## Decision

In master mode a thermostat WRITE_DATA of any ID except 1, 16 and 24 is sent to the boiler as a relay in the next free
master slot, ahead of the command ring and the schedule, and the thermostat gets the boiler's own answer through
`replyToThermostat()`, as in gateway mode; MsgID 1, 16 and 24, the IDs SAT owns while it is enabled, and every READ
keep their current handling.

1. **Forwarded.** Every thermostat WRITE_DATA except MsgID 1, 16 and 24. While `satOwnsControlSetpoint()` is true, the
   writes behind the commands SAT refuses from outside (`satBlocksExternalCommand()`: CS = MsgID 1, C2 = MsgID 8,
   MM = MsgID 14) keep the local echo and are not forwarded.
2. **A user override wins.** When a write override is in force for the ID (for example SW= on 56, SH= on 57), the frame
   goes out with the override's data (origin `OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN`, reply from
   `buildOverriddenReply()`), the gateway-mode rule, so the thermostat and the scheduled user write do not fight.
3. **Slot.** The pending thermostat frame waits only for a master request already in flight (`otMasterRequestActive`),
   then goes out before the command ring and the schedule; `scheduleMasterRequest()` starts no own frame while one
   waits.
4. **Answer.** The boiler's reply goes back unchanged: WRITE_ACK with the boiler's data, Data-Invalid, Unknown-DataID.
   A timeout or a corrupt reply gives no answer, and the thermostat retries (spec 4.5). The reply hold of TASK-1798
   (`OTD_REPLY_DEADLINE_MS`) counts from the thermostat frame's arrival on this path.
5. **Offline.** While the boiler is offline (TASK-1807) nothing is forwarded or answered.

## Decision Contract

### Must

- Send a thermostat WRITE_DATA of an ID other than 1, 16 and 24 to the boiler with the thermostat's data, or with the
  data of an active write override for that ID.
- Return the boiler's reply to the thermostat (WRITE_ACK, Data-Invalid, Unknown-DataID), never a local echo, for a
  forwarded write.
- Send the forwarded write before any queued command or scheduled frame, once the frame in flight has ended.
- Keep MsgID 1, 16 and 24 on today's local WRITE_ACK and write-cache path (TASK-1150, TASK-1811).
- Keep MsgID 1, 8 and 14 on the local echo while SAT owns the setpoint.

### Must Not

- Answer a forwarded write locally before or instead of the boiler.
- Abort or delay a frame already on the wire.
- Forward or answer anything while the boiler is offline.
- Change the handling of thermostat READs (ADR-066: answered from the cache).

### Exceptions

- None.

### Verification

- Host test on the real `handleMasterModeSlaveFrame()` and the master-mode consumer with a fake bus and fake time:
  a MsgID 56 write reaches the boiler with the thermostat's data and the thermostat gets the boiler's WRITE_ACK,
  Data-Invalid or Unknown-DataID; MsgID 1, 16, 24 and, with SAT active, 8 and 14 keep the local echo; an SW= override
  replaces the data of a 56 write; a write that arrives while an own frame is in flight goes out right after it,
  before the schedule. Red on the code before TASK-1812, green after.
- Bench where the hardware allows: a thermostat on the slave port of an OTGW32 in master mode with a boiler (or a
  boiler simulator); none on the current bench, so this is field-only until then.

## Alternatives Considered

- **Keep the local echo (today).** Rejected by Robert (2026-11-11): the thermostat sees an acknowledgement the boiler
  never gave, so a DHW setpoint or max CH setpoint from the thermostat silently does nothing.
- **Answer locally first, forward later.** Rejected: the thermostat would again get an answer the boiler never gave,
  the defect itself, and a boiler Data-Invalid or Unknown-DataID would never reach it.
- **Abort the gateway's own frame in flight for the write.** Rejected: the OpenTherm library cannot cancel a frame on
  the wire, and a cut frame is a frame error for the boiler (spec 4.5).
- **Forward only a fixed list of IDs (56, 57, 14).** Rejected: the PIC relays every write, and a list would need
  maintenance for every thermostat that writes another ID (8, 7, 9, 100, 20-22 seen in the audit).

## Consequences

**Positive:**

- A thermostat's DHW setpoint, max CH setpoint and other writes reach the boiler, and the thermostat learns when the
  boiler rejects one, as behind a PIC.
- Reuses the gateway-mode relay (`OT_DIRECT_ORIGIN_THERMOSTAT`, `replyToThermostat()`, the TASK-1798 hold).

**Negative:**

- A forwarded write is no longer an "instant response" (ADR-066): its answer comes after the frame in flight and the
  boiler's own answer time. With typical slaves (100 ms or less, spec :1095) that is a few hundred milliseconds; the
  spec's gateway clause (:1097-1098) allows the longer wait. Mitigation: a thermostat that times out first retries
  (spec 4.5); the boiler has the value already.
- The forwarded write takes a master slot, so the gateway's own schedule is delayed by one frame per thermostat write.
  Thermostats write rarely (a setpoint change), so the effect on the schedule is small.

## Open Questions

- [ ] Robert: accept the slot rule (relay in the next free master slot, ahead of the ring and the schedule) and the
  SAT exclusion of MsgID 8 and 14?

## Related Decisions

- **ADR-066 (Thermostat auto-detection and master mode)**: amended; its virtual boiler answers reads from the cache,
  and this ADR adds the write relay.
- **ADR-179 (Owner of the control setpoint while SAT is enabled)**: the SAT exclusion of MsgID 1, 8 and 14 follows it.

## References

- TASK-1812; audit `docs/reviews/2026-11-11_otdirect-pic-parity-audit/` (report E, U1); TASK-1798 (reply hold),
  TASK-1807 (offline), TASK-1150, TASK-1811 (MsgID 1).
- `src/OTGW-firmware/OTDirect.ino`: `handleMasterModeSlaveFrame()`, `replyToThermostat()`, `scheduleMasterRequest()`,
  `OTD_REPLY_DEADLINE_MS`; `src/OTGW-firmware/OTGW-firmware.h` `satBlocksExternalCommand()`.
- OpenTherm spec v4.2: :1093-1100 (answer window, gateway clause, MWT), 4.5 :1259-1266 (retry).
- PIC 6.6 `other-projects/otgw-6.6/gateway.asm`: the relay of thermostat frames in gateway mode (audit report E).
