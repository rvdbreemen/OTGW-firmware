---
id: TASK-1798
title: >-
  OT-Direct: answers the gateway makes itself are refused by the library's 50 ms
  slave gate, and the refusal is ignored
status: To Do
assignee: []
created_date: '2026-11-11T06:36:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 891000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report CD, Bug 3 (verified by the coordinator). After a received frame the library sets delayTimestamp = ts + RESPONSE_TIME * 1000 with RESPONSE_TIME 50 ms (src/libraries/OpenTherm/src/OpenTherm.cpp:443), and sendResponse() returns false while isReady() is false (OpenTherm.cpp:294-312, :136). OT-Direct sends its own answers in the same loop pass as the DATA_READY and ignores the return value: master mode handleMasterModeSlaveFrame (OTDirect.ino:2751), UI= Unknown-DataID (:2200), SR= answer (:2213), alternative-slot Unknown-DataID (:1875); otSlaveFramePending is cleared anyway. Only replyToThermostat checks and logs (OTDirect.ino:1484-1487), and that path comes late enough. PIC: its own answers leave after the boiler answered (SendAltResponse gateway.asm:2463-2473). Scenario: master mode with a thermostat, UI= or SR= set, or an alternative slot: the thermostat never gets an answer and times out. The host mock always returns true (tests/test_otd_gateway_alt_slots.py:186), so the existing tests cannot see this: test with the real sendResponse()/isReady() and a fake micros(). The vendored library is read-only: the fix belongs in OTDirect (keep the answer pending until the slave is ready). Related: TASK-1707 AC4 (bench with a thermostat), TASK-1178 note (d).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every answer the gateway makes itself (master mode, UI=, SR=, alternative slot) is sent once the slave is ready; no refused send is dropped silently (retried while the thermostat's window lasts, else logged)
- [ ] #2 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #3 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->
