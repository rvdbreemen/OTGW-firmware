---
id: TASK-1831
title: >-
  OT-Direct: VS= writes the ventilation setpoint in the wrong byte, once, and
  never overrides the thermostat's MsgID 71
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:37:00Z'
updated_date: '2026-11-12T01:25:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 914000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N1), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: VS= stores the value (G:4241-4254); every thermostat MsgID 71 request becomes a WRITE, value in LB, HB 0 (MessageID71 G:3210-3217). tests/ventilation-1.stc: VS=15 -> R9047000F; VS=XX -> `VS: -`. Spec v4.2:1514 "LB: Control Setpoint".
- OT-Direct: `data = setpt << 8` (HB) (O:3168; boot O:1019; OTDirecttypes.h:124 comment "MsgID 71 HB"). Own decoder reads LB (OTGW-Core.ino:5425). Goes out once via the queue; no refresh, no override of the thermostat frame.
- Consequence: VS=50 sends 0x3200, read as 0 %. A thermostat writing MsgID 71 overrules the command.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test on the real handler: VS=15 -> data 0x000F; red on current code
- [x] #2 With VS held, thermostat MsgID 71 READ/WRITE goes to the boiler as WRITE with the VS value; VS=T restores pass-through and replies `VS: -`
- [x] #3 Persisted setpoint applied after boot in gateway and master mode (closes TASK-1760)
- [ ] #4 Bench capture on the OT-Direct unit shows R.047.... with the value in LB
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): behaviour of a real ventilation unit.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Ground truth checked in this session: spec v4.2 MsgID 71 'LB: Control Setpoint ventilation / heat-recovery' u8 0..100 (docs/opentherm specification/...v4.2.md:1514-1518); PIC 6.8 MessageID71 (gateway.asm:3210-3217) sets MsgWriteOp, value via setbyte4 (LB), HB cleared; SetVentSetpoint (4241-4254) non-numeric -> ValueCleared; tests/ventilation-1.stc VS=15 -> R9047000F, VS=XX -> 'VS: -'. Fix: VS= data in LB; clear replies 'VS: -'; MsgID 71 gets a W_ENTRY and an otOverrides slot (both missing, so override and boot restore were no-ops, closes the TASK-1760 premise); applyOverrides turns a thermostat READ 71 into WRITE 71 with the VS value while VS is held; boot restore writes the LB value; OTDirecttypes.h comment says LB. Not changed: VS>100 is still clamped (PIC answers OutOfRange) - left for TASK-1804's range-check item.

Test tests/test_otd_vs_msgid71.py (real VS branch, enqueueWriteCommand, applyOverrides, set/clearOverride, updateWriteCache, clearWriteOverride, otOverrides table and the boot-restore block cut from OTDirect.ino): old code 6 FAIL, fix 6 OK (VS=15 sends 9047000F; thermostat WRITE 90470014 -> 9047000F as ventilation-1.stc; thermostat READ 71 -> WRITE 9047000F; VS=XX -> 'VS: -' and pass-through; persisted 40 % -> cache 0x0028 and the override after boot; the real schedule has W_ENTRY(71)). Neighbours OK: msgid99 6, tt_pic_parity 40, pr_reports 4, ps1_summary 9, gateway_alt_slots 12. evaluate --quick 92 checks 0 FAIL.

Deliberate deviation (recorded, not PIC): with W_ENTRY(71) and a VS value held, gateway mode also sends its own WRITE 71 in alternative slots when the thermostat does not write 71, and the persisted VS is written after every boot. The PIC only rewrites the thermostat's frames (or sends 71 with AA=71). Same model as the existing write entries 1/7/8/14/16/24/27/56/57 (TASK-1707 note a, alternative slots from the whole schedule, listed as an intentional difference in the 6.8 audit section 5). Needed for AC3 in master mode, where no thermostat frame exists. Bench on .61: loopback logs the gateway's R frame and the simulated B to the parser (sendMasterRequestAsync loopback branch), so R.047.... with the value in LB is visible there.
<!-- SECTION:NOTES:END -->
