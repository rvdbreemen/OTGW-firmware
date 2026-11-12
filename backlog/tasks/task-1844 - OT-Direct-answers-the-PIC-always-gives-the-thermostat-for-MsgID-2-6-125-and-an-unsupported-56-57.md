---
id: TASK-1844
title: >-
  OT-Direct: answers the PIC always gives the thermostat for MsgID 2, 6, 125 and
  an unsupported 56/57
status: To Do
assignee: []
created_date: '2026-11-11T23:56:00Z'
updated_date: '2026-11-12T00:11:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 923000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N10), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: MsgID 2 always acknowledged with the thermostat's own member ID and only the Smart Power bit (G:2837-2844; thermostat-3.stc AD002010B). MsgID 6 always READ_ACK with transfer-enable bits 0 and 1 set (G:2900-2918). MsgID 125: below 3.0 or no answer -> 3.0; new in 6.8, >=5.0 -> 4/51 (G:3327-3344; otversion-1/2/4.stc AC07D0300, A407D0433). MsgID 56/57 answered Unknown-DataID -> acknowledged with SW/SH value or upper bound, default 80/90 (G:3149-3161, :3185-3197, :262-265). These answers also come once blacklisted (fake answer via TreatMessage G:2467-2477).
- OT-Direct: replyToThermostat rewrites only MsgID 0, 9, 100 and overridden writes (O:1548-1582, :1490-1507). After 3 strikes the thermostat gets Unknown-DataID from the slot (O:1893-1897).
- Consequence: a thermostat behind a boiler without these IDs sees "not supported" and may stop offering DHW/max-CH setpoints.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host tests on the real reply path, one per ID, with the frames from the PIC test scripts; red on current code
- [ ] #2 same answers after 3 Unknown strikes
- [ ] #3 bench with slave simulator where possible
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): acknowledging Smart Power commits the slave interface to medium/high power; the library tracks it (OpenTherm.cpp:446, :540-544) but the hardware side was not checked. Decide MsgID 2 separately if in doubt.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
