---
id: TASK-1833
title: >-
  OT-Direct: CE= and H2= default to off where the PIC defaults to on; CL=0
  clears instead of holding 0 %
status: To Do
assignee: []
created_date: '2026-11-11T23:41:00Z'
updated_date: '2026-11-12T00:10:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 916000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N3), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: with CL= held, MsgID 0 bit 2 = !CoolModeOff; with C2= held, bit 4 = !CH2ModeOff (readstatus G:2720-2747). Both flags 0 after reset (RAM clear G:1178-1188) -> enable bit 1 until CE=0/H2=0 (G:4446-4452, G:5264-5270). Command ref "Initial value 1". ADR-229 lines 69-70 state the same.
- OT-Direct: coolEnable = otCoolingEnable default false (O:543, :917); ch2Enable = otMasterStatusFlags & 0x10 default 0 (O:64, :919, reset O:2680-2682).
- CL=0: PIC holds 0 % (G:4454-4464; only a bad value clears, reply `-`). OT-Direct clears the override (O:3294-3307).
- Consequence: C2=40 without H2=1 switches the CH2 enable bit off; on the PIC it is forced on. Same for cooling.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test on real otdRelayedStatusHB path: after C2=40 only bit 4 = 1; after CL=50 only bit 2 = 1; H2=0/CE=0 clear them; red on current code
- [ ] #2 CL=0 holds 0 %; CL=x clears and replies `CL: -`
- [ ] #3 ADR-229 note/amendment naming the defaults
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): master-mode status bits (ADR-066, separate design).

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
