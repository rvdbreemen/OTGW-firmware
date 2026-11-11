---
id: TASK-1812
title: >-
  OT-Direct master mode: forward the thermostat's WRITEs of other IDs (56, 57,
  14, ...) to the boiler and return the boiler's answer, like the PIC
status: To Do
assignee: []
created_date: '2026-11-11T12:30:00Z'
updated_date: '2026-11-11T12:31:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - feature
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 905000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report E, U1. Robert's decision (2026-11-11, AskUserQuestion in the coordinator session): "Forward to the boiler": thermostat WRITEs of other IDs (56/57/14/...) in master mode are forwarded as a boiler write and the boiler's answer goes back to the thermostat (like the PIC). Code today (dev 755203ef0): handleMasterModeSlaveFrame (OTDirect.ino:2801-2848) answers every thermostat WRITE_DATA with a WRITE_ACK echo of the thermostat's own data (:2817-2819) and acts only on MsgID 1, 16 and 24 (:2825-2836). Every other write (e.g. 56 DHW setpoint, 57 max CH setpoint, 14 max modulation, 8, 7, 9, 100, 20-22) is acknowledged and dropped, so the thermostat believes its DHW setpoint or max-CH value is set while the boiler never saw it. The PIC relays the thermostat's frame and returns the boiler's reply (WRITE_ACK, Data-Invalid or Unknown-DataID). Design points for the implementer: the slave answer must leave within the OpenTherm response window (spec v4.2: 20-800 ms), while the master port runs its own schedule, so state how the forwarded write and its answer fit that window (a relay in the same slot, as gateway mode does, or another choice with its reason); keep MsgID 1 (TASK-1150, and the TSet expiry task filed with this one), 16 and 24 as they are unless the design says otherwise; keep the SAT ownership rules. ADR-066 (hybrid master mode) does not describe thermostat writes: check with /adr-kit:context whether this needs a new ADR (Proposed, never self-accepted).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In master mode a thermostat WRITE of an ID other than 1, 16 and 24 is sent to the boiler as a WRITE_DATA with the thermostat's data
- [ ] #2 The thermostat receives the boiler's own answer to that write (WRITE_ACK with the boiler's data, Data-Invalid or Unknown-DataID), not a local echo; the answer stays within the OpenTherm response window or the task notes show why it cannot and what was chosen instead
- [ ] #3 MsgID 1, 16 and 24 and the SAT ownership rules behave as before this change, shown by a test
- [ ] #4 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #5 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
The TSet expiry task filed together with this one is TASK-1811.
<!-- SECTION:NOTES:END -->
