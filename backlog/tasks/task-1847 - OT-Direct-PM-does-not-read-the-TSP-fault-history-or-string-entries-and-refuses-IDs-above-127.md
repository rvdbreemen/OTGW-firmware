---
id: TASK-1847
title: >-
  OT-Direct: PM= does not read the TSP, fault-history or string entries, and
  refuses IDs above 127
status: To Do
assignee: []
created_date: '2026-11-12T00:01:00Z'
updated_date: '2026-11-12T00:11:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 926000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N13), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: after PM=10 (also 12, 88, 90, 105, 107) the answer's count starts reads of every entry of the next ID (G:3364-3378, :3389-3397, index G:3464-3470); PM=93/94/95 reads every string character (G:3399-3407); range 0-255 (G:5026-5037). tests/tsp-1.stc, str-1.stc. Command ref describes this.
- OT-Direct: one READ with data 0, or a schedule entry forced due; IDs > 127 -> BV (O:3610-3632).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test: PM=10 against a mock boiler with 3 TSPs produces the R frames of tsp-1.stc; red on current code
- [ ] #2 PM=93 reads all characters
- [ ] #3 PM=131 accepted
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): interaction with the PM slot rule of TASK-1572 and TASK-1767.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
