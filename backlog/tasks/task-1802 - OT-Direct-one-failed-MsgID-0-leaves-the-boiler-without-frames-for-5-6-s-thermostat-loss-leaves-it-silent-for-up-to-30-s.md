---
id: TASK-1802
title: >-
  OT-Direct: one failed MsgID 0 leaves the boiler without frames for 5-6 s;
  thermostat loss leaves it silent for up to 30 s
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T06:40:00Z'
updated_date: '2026-11-14T21:41:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 895000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report CD, Bug 1 and Bug 9. Bug 1 (master mode): any non-SUCCESS answer to MsgID 0 sets bOnline = false (OTDirect.ino:1680-1689); scheduleMasterRequest then sends only MsgID 0, every OT_OFFLINE_RETRY_MS = 5000 ms (:58, :1731-1750, :1778). With the 1 s library timeout that is 5 to 6 s without a frame. PIC: no offline state; the same slot is retried at the next 1 s tick (gateway.asm:1490-1498, :1549-1564), and its comment :1702-1721 names the risk. Spec v4.2 (docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md): the master communicates at least every 1.15 s (:1101); a low line with no valid frame for 5 s or more may be treated as a short circuit = heat demand (:1006-1016). Bug 9 (gateway mode): while bThermostatConnected is still true nothing is sent (:1712, :861-863) until checkThermostatTimeout declares the thermostat gone after iSetbackTimeout (default 30 s, OTDirecttypes.h:111; :2654-2690). The PIC detects the open line electrically within tens of ms (CheckThermostat :1728-1816). Not verified on a boiler. The decision task 'remove the 16 C setback' changes what happens after the timeout, not the timeout itself.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a failed MsgID 0 in master mode the next frame goes out within 1.15 s
- [ ] #2 The gap between the thermostat's last frame and the gateway's first own frame stays within the spec's 5 s, or the remaining gap is a recorded decision by Robert; both gaps measured in a host test with fake time
- [ ] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bug 1 fix (d3, lane-1924): scheduleMasterRequest() keeps sending only MsgID 0 while the bus is offline, but at its normal OT_STATUS_INTERVAL_MS (800 ms) instead of OT_OFFLINE_RETRY_MS (5000 ms, removed). With the 1 s library timeout (src/libraries/OpenTherm/src/OpenTherm.cpp:457) the next frame now goes out about 1.0 s after a failed one; the library's 100 ms wait (MWT, spec :1101) makes it about 1.1 s on the wire, within the spec's 1.15 s. PIC: no offline state, the next 1 s tick retries (gateway.asm 6.6 :1490-1498).

AC1/AC3 evidence for Bug 1: new tests/test_otd_offline_retry.py compiles the real scheduleMasterRequest() and the real scheduler constants with fake time and a fake bus (1 s timeout for a silent boiler, 50 ms for an answer). Fix: OK, 'dead sent=30 maxgap=1010', 'blip sent=41 maxgap=1010'. On HEAD 288dc65ba: FAILED (failures=2), 'dead sent=6 maxgap=5000', 'blip sent=36 maxgap=5000'. All tests/test_otd*.py OK, test_sat_no_return_startup 20/20 OK, evaluate.py --quick 0 failed. Not modelled: the library's own 100 ms MWT (adds ~0.1 s). docs/c4/c4-code-otdirect.md updated.

Bug 9 (gateway: thermostat lost, no own frame until iSetbackTimeout 30 s) is NOT in this commit. The gap is not only timing: once the gateway sends its own MsgID 0 the CH enable comes from otMasterStatusFlags (board default bit 0 on), so starting own frames early would turn CH on for a thermostat that had it off. The PIC clears HeatDemand and all thermostat state on an open line (gateway.asm 6.6 Disconnect :1770-1789) before it sends StandAloneMsg (:1796). What the boiler gets after the loss is TASK-1806's subject (remove the 16 C setback, like the PIC); Bug 9 goes with 1806. ba's TASK-1850 also edits checkThermostatTimeout.
<!-- SECTION:NOTES:END -->
