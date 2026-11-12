---
id: TASK-1750
title: >-
  OT-Direct: MsgID 100 remote override flags like the PIC (TT b'11', OH= high
  byte)
status: To Do
assignee: []
created_date: '2026-11-09T09:28:00Z'
updated_date: '2026-11-12T00:07:00Z'
labels:
  - otdirect
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 852000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). The PIC answers MsgID 100 with the function bits of the active override: TT= b'11' (manual and program change), TC= b'01' (gateway.asm 6.6:4053-4063, MessageID100 ~3285-3289), copied into the high byte when OH= (OverrideHigh, default on) is set. OT-Direct answers TT= with 0x0002 and never applies OH= (otOverrideHB, OTDirect.ino ~520, ~3566) to the MsgID 100 answer (~1432). Under ADR-223 this answer is the thermostat's only channel, so the flags matter more. Decide with Robert whether to move to the PIC values (behaviour change for thermostats that read MsgID 100).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides: keep 0x0002 or move to PIC parity (TT 0x03, OH= high byte)
- [ ] #2 If parity: MsgID 100 answers match gateway.asm for TT=, TC= with OH on/off; host test red before, green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (AC gap): description says OH defaults on, ACs do not. PIC default IgnoreErr1 and OverrideHigh set (G:5459) -> `PR: T=11` (tests/serial-2.stc). OT-Direct otOverrideHB=0 (O:535) -> T=10.
<!-- SECTION:NOTES:END -->
