---
id: TASK-1846
title: 'OT-Direct: PS=1 does not show the OT= value in the MsgID 27 field'
status: To Do
assignee: []
created_date: '2026-11-11T23:59:00Z'
updated_date: '2026-11-12T00:11:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 925000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N12), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: OT= writes the value straight into the summary store (StoreOutTemp G:4276-4287, slot G:568-569, index 15 = MsgID 27).
- OT-Direct: store filled only from frames (T WRITE, B READ_ACK, R WRITE without thermostat; O:2097-2106). With a thermostat connected OT=7.5 never reaches field 16 (O:3141-3153). Rest of the line matches the PIC print routines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test: thermostat connected, OT=7.5 then PS=1 -> field 16 = 7.50; red on current code
- [ ] #2 line stays byte-identical to tests/summary-1.stc for the same frames
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): for an ID with an SR= entry the PIC still stores the boiler's answer while OT-Direct sends no boiler frame (TASK-1707 note b), so that field can differ too.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
