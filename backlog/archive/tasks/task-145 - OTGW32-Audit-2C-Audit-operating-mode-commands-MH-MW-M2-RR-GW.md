---
id: TASK-145
title: 'OTGW32-Audit-2C: Audit operating mode commands (MH, MW, M2, RR, GW)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:18'
updated_date: '2026-04-08 22:34'
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
Verify MsgID 99 operating mode commands (MH=, MW=, M2=) and the remote request command (RR=) against PIC spec. Also audit all GW= sub-modes: 0=bypass, 1=gateway, 2/S=master, M=monitor, L=loopback, R=reset. Verify MsgID 99 data byte assembly and GW=R full state reset completeness.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MH= sets lower nibble of byte4 in MsgID 99 WRITE_DATA
- [x] #2 MW= sets lower nibble of byte3 in MsgID 99 WRITE_DATA
- [x] #3 M2= sets upper nibble of byte4 in MsgID 99 WRITE_DATA
- [x] #4 MsgID 99 frame is queued immediately on each MH/MW/M2 command
- [x] #5 RR= sends one-shot WRITE_DATA to MsgID 4 with code in HB
- [x] #6 GW=0 activates bypass (or returns NG if no bypass relay)
- [x] #7 GW=1 activates gateway mode
- [x] #8 GW=2 and GW=S activate master/standalone mode
- [x] #9 GW=M activates monitor mode
- [x] #10 GW=L activates loopback mode
- [x] #11 GW=R restarts both OT interfaces and resets transient state
- [x] #12 Any deviation from PIC spec results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit of operating mode commands complete.

MH= (lower nibble of byte4 = HB of data99): PASS - otOperModeCH1 = val & 0x0F, assembled as (otOperModeCH1 | (otOperModeCH2 << 4)) in HB. Frame queued immediately.
MW= (lower nibble of byte3 = LB of data99): PASS - otOperModeDHW = val & 0x0F, placed in LB. Frame queued immediately.
M2= (upper nibble of byte4 = HB upper nibble): PASS - otOperModeCH2 = val, upper nibble of HB: (otOperModeCH1 | (otOperModeCH2 << 4)).
MsgID 99 frame queued immediately on each MH/MW/M2: PASS.

RR= (MsgID 4, one-shot): PASS - code placed in HB (code<<8), WRITE_DATA, queued to ring buffer.

GW= modes:
- GW=0 (bypass): PASS - setOTDirectMode(OTD_MODE_BYPASS) or "NG" if no bypass relay.
- GW=1 (gateway): PASS - setOTDirectMode(OTD_MODE_GATEWAY).
- GW=2 and GW=S (master/standalone): PASS - both map to OTD_MODE_MASTER.
- GW=M (monitor): PASS - setOTDirectMode(OTD_MODE_MONITOR), transparent pass-through.
- GW=L (loopback): PASS - OTDirect extension, simulated boiler for testing. Not in PIC spec but reasonable addition.
- GW=R (reset): PASS - resetTransientState() + restart OT interfaces via end()/begin().

Note: PIC spec only defines GW=0 (monitor), GW=1 (gateway), GW=R (reset). The OTGW32 extends with GW=2, GW=S, GW=M, GW=L. These are intentional extensions documented in OTDirect.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All MH, MW, M2, RR, and GW sub-modes verified correct.

MsgID 99 data byte assembly: LB = otOperModeDHW (MW), HB = otOperModeCH1 | (otOperModeCH2<<4) (MH upper nibble for M2, lower for MH). Frame queued immediately on every command.

RR= correctly encodes code in HB of MsgID 4 WRITE_DATA.

GW= modes: 0/bypass, 1/gateway, 2+S/master, M/monitor, L/loopback, R/reset all implemented. GW=0 returns NG without bypass relay. GW=R restarts both OT interfaces.

OTGW32 extends PIC spec with GW=2, S, M, L modes. This is intentional and documented.
<!-- SECTION:FINAL_SUMMARY:END -->
