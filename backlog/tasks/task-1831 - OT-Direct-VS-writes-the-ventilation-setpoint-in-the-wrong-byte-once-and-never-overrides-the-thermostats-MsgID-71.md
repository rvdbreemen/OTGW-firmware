---
id: TASK-1831
title: >-
  OT-Direct: VS= writes the ventilation setpoint in the wrong byte, once, and
  never overrides the thermostat's MsgID 71
status: To Do
assignee: []
created_date: '2026-11-11T23:37:00Z'
updated_date: '2026-11-12T00:09:00Z'
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
- [ ] #1 host test on the real handler: VS=15 -> data 0x000F; red on current code
- [ ] #2 With VS held, thermostat MsgID 71 READ/WRITE goes to the boiler as WRITE with the VS value; VS=T restores pass-through and replies `VS: -`
- [ ] #3 Persisted setpoint applied after boot in gateway and master mode (closes TASK-1760)
- [ ] #4 Bench capture on the OT-Direct unit shows R.047.... with the value in LB
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): behaviour of a real ventilation unit.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
