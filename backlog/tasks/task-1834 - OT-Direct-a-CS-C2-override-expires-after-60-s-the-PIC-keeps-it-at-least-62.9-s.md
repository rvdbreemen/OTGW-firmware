---
id: TASK-1834
title: >-
  OT-Direct: a CS=/C2= override expires after 60 s; the PIC keeps it at least
  62.9 s
status: To Do
assignee: []
created_date: '2026-11-11T23:43:00Z'
updated_date: '2026-11-12T00:10:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 917000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N4), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: expiry counts 121 Timer-1 overflows of 524.288 ms (G:157-158, :1104-1112, StartTimer G:4233-4239, CommandExpiry G:1623-1646): lives 62.9 to ~126 s after last command. tests/control-3.stc: CS at 2 s still active at 32 s, gone between 65 and 74 s. Command ref "repeated at least every minute".
- OT-Direct: OT_CSC2_EXPIRY_MS = 60000, checked once a second (O:499-501, :2421-2431).
- Consequence: a client refreshing exactly every 60 s never lapses on the PIC; on OT-Direct the override can drop just before the refresh so the thermostat TSet reaches the boiler briefly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test with fake clock on real expiry code: last CS= at t=0 still active at t=62 s; red on current code
- [ ] #2 expiry not earlier than 63 s for CS and C2
- [ ] #3 bench: CS= every 60 s for 15 min, no thermostat-valued MsgID 1 in the capture
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): copy the full 63-126 s window or only the lower bound (Robert's call). NOTE: interacts with TASK-1811 and with SAT's 5 s CS resend (ADR-203/207, SAT exempt from CS expiry per TASK-1150).

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
