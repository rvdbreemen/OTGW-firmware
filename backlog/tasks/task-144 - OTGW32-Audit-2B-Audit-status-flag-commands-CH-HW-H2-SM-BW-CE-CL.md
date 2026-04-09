---
id: TASK-144
title: 'OTGW32-Audit-2B: Audit status flag commands (CH, HW, H2, SM, BW, CE, CL)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:17'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-2
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify all master status flag commands against the PIC firmware specification. Each command maps to a specific bit in the MsgID 0 master status byte sent every 800ms. Check bit positions, value parsing (0/1/A/P for HW=), and that the status byte is correctly included in every MsgID 0 WRITE_DATA frame.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CH= sets/clears bit0 of master status
- [x] #2 HW= sets/clears bit1, supports 0/1/A/P values, P triggers DHW push
- [x] #3 H2= sets/clears bit4 of master status
- [x] #4 SM= sets/clears bit5, persisted to settings.otd.bSummerMode
- [x] #5 BW= sets/clears bit6, not persisted
- [x] #6 CE= sets/clears bit2, not persisted
- [x] #7 CL= is alias for CC= (MsgID 7), value 0 clears override
- [x] #8 Master status byte is included in every MsgID 0 frame sent to boiler
- [x] #9 Any deviation from PIC spec results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit of status flag commands complete.

CH= (bit0): PASS - sets/clears bit0 of otMasterStatusFlags, strict 0/1 validation via checkBoolean().
HW= (bit1): PASS - supports 0/1/A/P values. P triggers DHW push state machine with MsgID 99 (bit4 of DHW byte). 0 aborts in-progress push. A sets auto mode.
H2= (bit4): PASS - sets/clears bit4 (0x10) of master status, strict 0/1.
SM= (bit5): PASS - sets/clears bit5 (0x20), persisted to settings.otd.bSummerMode. Correct.
BW= (bit6): PASS - sets/clears bit6 (0x40), not persisted (otDHWBlocking local only).
CE= (bit2): PASS - sets/clears bit2 (0x04), not persisted.
CL= (MsgID 7 alias): PASS - alias for CC= with clear-on-zero behavior. CL=0 clears override; CC= does not clear on zero. This matches PIC behavior where CL= and CC= are slightly different.

MsgID 0 frame uses otMasterStatusFlags in HB: buildStatusRequest() constructs MsgID 0 READ_DATA with (otMasterStatusFlags<<8). This is sent every 800ms via the schedule table. PASS.

All 9 ACs verified.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All 7 status flag commands (CH, HW, H2, SM, BW, CE, CL) verified correct.

- Bit positions: CH=bit0, CE=bit2(0x04), HW=bit1(0x02), H2=bit4(0x10), SM=bit5(0x20), BW=bit6(0x40).
- HW= full state machine: 0/1/A/P with DHW push via MsgID 99.
- SM= persists to settings.otd.bSummerMode; BW= and CE= are session-only.
- CL= correctly aliases CC= (MsgID 7) with clear-on-zero behavior.
- MsgID 0 frame built by buildStatusRequest() using otMasterStatusFlags in HB, sent every 800ms.

No deviations from PIC spec.
<!-- SECTION:FINAL_SUMMARY:END -->
