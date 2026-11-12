---
id: TASK-1849
title: 'OT-Direct: success and clear replies that differ from the PIC'
status: To Do
assignee: []
created_date: '2026-11-12T00:04:00Z'
updated_date: '2026-11-12T00:12:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 928000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N15), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- SC=: PIC two-digit hours `SC: 09:05/3` (G:4374-4388, PrintDecimal G:2086-2101); OT-Direct %d (O:3188).
- SR=: PIC `18:1/230`, `70:0/14` (G:4856-4859); OT-Direct echoes input (O:3693).
- FT=: PIC prints D, S, R, C or I (G:4705-4727); OT-Direct echoes any character (O:3901-3905).
- VS= clear: PIC `VS: -` (G:4253-4254); OT-Direct echoes input (O:3158-3160).
- CL= clear: see N3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test per reply, byte-exact; red on current code
- [ ] #2 one line per item in the final summary
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): client impact; none found.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
