---
id: TASK-1832
title: 'OT-Direct: MsgID 99 data bytes are swapped (MW=, MH=, M2=, HW=P)'
status: To Do
assignee: []
created_date: '2026-11-11T23:39:00Z'
updated_date: '2026-11-12T00:09:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 915000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N2), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: HB = DHW mode (low nibble) + manual push (bit 4); LB = HC1 (low nibble), HC2 (high nibble). OperModeData G:3247-3250, SetOperModeDHW G:4482-4494, SetOperModeHC1 G:4466-4478, SetOperModeHC2 G:5250-5262. tests/dhw-5.stc HW=P -> R10631000; opermode-1.stc MH=2 -> A40630002. Spec v4.2:2249-2277 same.
- OT-Direct: `data99 = DHW | (CH1 | CH2<<4) << 8` (O:3208-3209, :3319, :3332, :3345): DHW and push in LB, heating in HB. Own decoder uses PIC layout (OTGW-Core.ino:3629-3634).
- Consequence: MW=4 tells the boiler "HC1 reduced"; HW=P sets HC2=Auto and never requests a push.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test on real handlers: MW=4 -> 0x0400, MH=2 -> 0x0002, M2=3 -> 0x0030, HW=P -> bit 12; red on current code
- [ ] #2 decoded MQTT topics for MsgID 99 match the command sent
- [ ] #3 bench capture of the R frames
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): boiler reaction.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
