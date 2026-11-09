---
id: "ADR-223"
title: "OT-Direct gateway mode answers a TT= or TC= override to the thermostat only, like the PIC"
status: "Proposed"
date: "2026-11-09"
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
  - "remote-override"
  - "room-setpoint"
  - "pic-parity"
aliases:
  - "TT= on OT-Direct"
  - "temporary room setpoint override"
  - "MsgID 9 remote override"
  - "TASK-1733 F2"
  - "roomsetptdiff"
components:
  - "OT-Direct TT= remote override in gateway mode"
symbols:
  - "setRemoteOverride"
  - "clearRemoteOverride"
  - "onThermostatMsgID16"
  - "otdRemoteOverrideReply"
  - "otOverrides"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-223 OT-Direct gateway mode answers a TT= or TC= override to the thermostat only, like the PIC

## Status

Proposed, 2026-11-09.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-11-09, in session
otgw-firmware-80 (TASK-1733: option F2 "PIC parity now", and the four parity sub-points below).
For `TC=` (SAT push-setpoint): Sergeant D, 2026-11-09 06:06 UTC, answer "TASK-1733: A" in
#dev-sat-mqtt (https://discord.com/channels/812969634638725140/1105556725714649128/1569231775144607755),
recorded as corrections entry 286.

## Status History

```yaml
status_history:
  - date: 2026-11-09
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

`TT=` sets a temporary room setpoint override (the thermostat's own program may replace it at its
next program change). The PIC gateway firmware (PIC: the OpenTherm Gateway's microcontroller,
reference source `other-projects/otgw-6.6/gateway.asm`, version 6.6) implements it purely as an
answer to the thermostat. On `TT=` it stores the value and function bits b'11'
(`gateway.asm:4058-4063`) and then:

- answers the thermostat's READ of MsgID 9 (remote override room setpoint) with the value and the
  READ of MsgID 100 (remote override function) with the flags, rewriting the boiler's reply
  (`gateway.asm:2932-2955`, `3269-3303`);
- never sends MsgID 9, 16 or 100 to the boiler itself (the IDs appear only at their definitions,
  `gateway.asm:242,244,256`), and passes the thermostat's WRITE of MsgID 16 (room setpoint) to the
  boiler unchanged (`gateway.asm:2976-2985`);
- compares the thermostat's MsgID 16 with the override only after the thermostat has read MsgID 9
  (`OverrideWait` is cleared only in the MsgID 9 reply path, `gateway.asm:2948`; the compare is
  skipped while it is set, `gateway.asm:2980-2982`). A value within about ±0.125 °C is a match
  (`gateway.asm:2986-3005`). Before the thermostat accepts, the 4th mismatching frame cancels the
  override; after acceptance the first mismatch cancels it (`roomsetptdiff`, `gateway.asm:3025-3036`).

OT-Direct (the ESP32-S3 board that is the OpenTherm master/slave itself, without a PIC) does more
today. `setRemoteOverride()` queues a gateway WRITE 16 with the override value and a WRITE 100 with
the flags to the boiler (`src/OTGW-firmware/OTDirect.ino:2355`, `:2366`), registers MsgID 16 in
`otOverrides` (`OTDirect.ino:425`) so every thermostat WRITE 16 is rewritten to the override value
(`applyOverrides`, `OTDirect.ino:630-645`), and refreshes the write every 15 s through the write
cache. `clearRemoteOverride()` also clears the shared MsgID 16 and 100 write overrides
(`OTDirect.ino:2392-2393`), so `TT=0` also wipes a `BS=` fake setpoint. `onThermostatMsgID16()`
releases a TT after 3 honoured frames and a mismatch above 0.5 °C (`OTDirect.ino:2419-2457`,
constants `:464-466`). The answers to MsgID 9/100 reads are already PIC-like (TASK-1718,
`OTDirect.ino:1397-1450`).

Trigger: tester tjfs (beta.146, OT-Direct, SAT off) sent `TT=21.5`; the boiler did not fire and the
UI showed confusing values (TASK-1733). A code analysis (not a reproduction: his capture is not
available) found that TT= on OT-Direct never changes CH enable or the control setpoint, so "the
boiler does not fire" is expected, and that OT-Direct's extra boiler writes are not what a PIC does.

## Decision

**User Decision:** in OT-Direct **gateway mode**, a `TT=` or `TC=` override is answer state only, as
on the PIC: the gateway answers the thermostat's MsgID 9 and MsgID 100 reads, sends no WRITE 16 or
WRITE 100 of its own to the boiler for it, passes the thermostat's WRITE 16 unchanged, and releases
the override by the PIC rule once the thermostat has read MsgID 9. The PIC treats `TT=` and `TC=`
the same in this rule (MessageID16 has no TT/TC branch, `gateway.asm:2976-3036`); only the MsgID 100
function bits differ (b'11' for TT, b'01' for TC, `gateway.asm:4053-4063`).

The parity sub-points, decided by Robert on 2026-11-09:

1. **Counting starts after a MsgID 9 read** (strict PIC). A thermostat that never reads MsgID 9
   never releases the override; it stays active until `TT=0`, as on a PIC board.
2. **TT and TC alike.** `TC=` is SAT's push-setpoint on OT-Direct, so this changes SAT behaviour;
   Sergeant D chose option A on 2026-11-09 (corrections entry 286): `TC=` behaves like the PIC.
   Entry 16 stands: SAT does not re-push a refused setpoint. This updates entry 14's Open note
   ("OT-Direct boards never cancel a TC= override").
3. **Match tolerance stays 0.25 °C** (`OT_OVERRIDE_HONOR_DELTA_F88`), a documented deviation from
   the PIC's ~0.125 °C so that thermostats rounding to 0.5 °C steps are not cancelled.
4. **Master mode is unchanged:** with no thermostat, `TT=` keeps writing TrSet (MsgID 16) to the
   boiler, because nobody would read MsgID 9.

## Decision Contract

### Must

- In gateway mode, a `TT=` or `TC=` value is delivered to the thermostat only through the MsgID 9
  and MsgID 100 read answers (`otdRemoteOverrideReply`).
- After the thermostat's first answered MsgID 9 read, a thermostat MsgID 16 within 0.25 °C of the
  override counts as acceptance; before acceptance the 4th mismatch releases the override, after
  acceptance the first mismatch releases it.
- `TT=0` and a release clear only the remote override state; a `BS=` or MQTT room setpoint write
  override on MsgID 16 survives.

### Must Not

- No gateway-originated WRITE 16 or WRITE 100 to the boiler for a `TT=` or `TC=` in gateway mode.
- No rewrite of the thermostat's WRITE 16 data for a `TT=` or `TC=`.
- No change to SAT control (CS/CH/MM) under this ADR; no SAT re-push of a refused `TC=` (entry 16).

### Exceptions

- OT-Direct master mode (no thermostat): `TT=` and `TC=` keep writing TrSet to the boiler.

### Verification

- New host test `tests/test_otd_tt_pic_parity.py` on the real `handleOTDirectCommand("TT=21.5")`,
  `setRemoteOverride`, `applyOverrides`, `onThermostatMsgID16` and `otdRemoteOverrideReply` in the
  gateway loop of `tests/test_otd_gateway_alt_slots.py`: no gateway WRITE 16/100, unchanged
  thermostat WRITE 16, MsgID 9 answered 0x1580, release after 4 mismatches once MsgID 9 was read;
  the same cases for `TC=21.5` (MsgID 100 flags 0x01). Red on the code before this ADR, green after.

## Alternatives Considered

- **F1: keep the boiler writes, fix only the false confirmation** (report `TT: 21.50` only when the
  write was queued). Rejected by Robert: it keeps OT-Direct's behaviour different from the PIC that
  the OpenTherm tooling (OTmonitor, thermo-nova, Home Assistant) was written against.
- **Count mismatches from the moment `TT=` is set.** Rejected: deviates from the PIC
  (`gateway.asm:2980-2982`); for a thermostat that never reads MsgID 9 the override is harmless
  under this ADR, because nothing is written to the boiler.
- **`TT=` only, `TC=` unchanged.** Rejected after Sergeant D chose option A (entry 286): one
  behaviour on PIC and OT-Direct boards for SAT's push-setpoint.
- **Push SAT's setpoint with `TT=` instead of `TC=`** (as thermo-nova's serial path does, pyotgw
  `set_target_temp` defaults to temporary). Rejected by Sergeant D (option C of Q-1733a).
- **PIC tolerance 0.125 °C.** Rejected: cancels thermostats that report in 0.5 °C steps.
- **Do nothing.** Rejected: OT-Direct would keep writing a room setpoint to the boiler that a PIC
  never writes, and keep rewriting the thermostat's own MsgID 16.

## Consequences

**Positive:**

- OT-Direct gateway mode behaves like a PIC gateway for `TT=`, so OpenTherm tools and testers see
  the same frames on both boards.
- The boiler's MsgID 16 always carries the thermostat's own room setpoint; one fewer gateway write
  every 15 s on the 12-frame write queue for an active TT.
- `TT=0` no longer clears an unrelated `BS=` fake setpoint.
- The false `TT: 21.50` confirmation after a refused queue write disappears, because the TT path no
  longer queues a write.

**Negative:**

- With a thermostat that never reads MsgID 9, `TT=` has no effect at all and the override stays
  shown as active until `TT=0`, as on a PIC. Mitigation: documented here and in the release note;
  MQTT `otgw32/room_setpoint` remains the explicit way to write TrSet.
- SAT's push-setpoint (`TC=`) on OT-Direct is now cancelled by a thermostat that keeps its own
  setpoint after reading MsgID 9, as on PIC boards, and SAT does not re-push it (entry 16).
  Mitigation: same behaviour as PIC boards, chosen by Sergeant D (entry 286).
- ADR-068's rationale lists TT/TC as producers of WRITE 16/100 frames; that rationale is outdated
  for `TT=` in gateway mode (no hard conflict: ADR-068's decision, the 12-frame queue, stands).

## Open Questions

- [x] Sergeant D: should `TC=` (SAT push-setpoint) follow the same PIC rule on OT-Direct? — **Answered 2026-11-09 by Sergeant D:** "TASK-1733: A": yes, like the PIC (#dev-sat-mqtt msg 1569231775144607755, corrections entry 286).

## Related Decisions

- **ADR-068** (12-frame OT-Direct write queue): its rationale names TT/TC as WRITE 16/100 producers;
  this ADR removes the TT producer in gateway mode. The queue decision stands.
- **ADR-118** (override store): after this ADR a TT on OT-Direct is visible only as the answered
  MsgID 9/100 frames, not as a substituted WRITE 16.
- **ADR-208** (CH= override on the relayed MsgID 0): same pattern of gateway answers in OT-Direct
  gateway mode; not changed.

## References

- TASK-1733 (notes: code analysis, PIC research, Robert's decisions of 2026-11-09).
- `other-projects/otgw-6.6/gateway.asm`: 242-256, 2932-2955, 2976-3036, 3269-3303, 4053-4063.
- `src/OTGW-firmware/OTDirect.ino`: 420-431 (`otOverrides`), 464-466 (tolerances), 630-645
  (`applyOverrides`), 1397-1450 (MsgID 9/100 answers), 2345-2381 (`setRemoteOverride`), 2382-2404
  (`clearRemoteOverride`), 2419-2457 (`onThermostatMsgID16`).
- `docs/sat/sergeant-d-corrections.md` entries 14, 16 and 286 (push-setpoint, `TC=`).
- thermo-nova at 7beaaf5: `climate.py:656-657`, `coordinator/mqtt/opentherm.py:206-209` (`TC=`),
  `coordinator/serial/__init__.py:204-208` with pyotgw `pyotgw.py:71-83` (`TT=` by default).
