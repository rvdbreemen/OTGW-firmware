---
id: TASK-1798
title: >-
  OT-Direct: answers the gateway makes itself are refused by the library's 50 ms
  slave gate, and the refusal is ignored
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:36:00Z'
updated_date: '2026-11-11T19:29:00Z'
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
- [x] #1 Every answer the gateway makes itself (master mode, UI=, SR=, alternative slot) is sent once the slave is ready; no refused send is dropped silently (retried while the thermostat's window lasts, else logged)
- [x] #2 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #3 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
One holder for an answer to the thermostat in OTDirect (otdSendReply/otdReplyService): send now, or keep it and send from loopOTDirect() once the library's 50 ms slave gate opens (OpenTherm.cpp:443, sendResponse false while !isReady); drop and log it on the next thermostat frame, a mode switch, or after 750 ms (OT slave window 800 ms). All six answer sites use it (master mode, UI=, SR=, alternative slot, MsgID 28 slot, relayed reply). The relayed reply logs its A only when it actually leaves (C28 contract of test_override_reply). Library stays untouched.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (wt-cyc09): tests/test_otd_slave_reply_gate.py runs the gateway loop of the alt-slots harness, a real handleMasterModeSlaveFrame() and the library's real isReady()/sendResponse() bodies, RESPONSE_TIME and DATA_READY delay statement (cut from OpenTherm.cpp) on a fake micros(). Old code (HEAD d9eda734d copy): G1 alternative slot 0/60 answered, G2 UI= 0/30, G3 SR= 0/30, G4 master mode 0/30; G0 relayed reply 30/30 (90 ms boiler round trip, contrast). Fix: all answered within 800 ms. test_override_reply C28 (refused reply: no A for it) kept: the relayed path logs A only once the held reply leaves. Regression: all tests/test_otd_*.py OK, test_override_reply / test_local_frame_consumers / test_boiler_unsupported_origin PASS, evaluate.py --quick 0 FAIL. Residual by design: a GW=R in the same mode can still send a held reply on the restarted slave within 750 ms (resetTransientState() may run on the async_tcp task, so it does not touch the holder).

Refute-only review (opus): no stale or duplicate send in the normal flow. Fixed from it: (1) the service checked the 750 ms deadline only after a refused send, so after a loop stall over ~800 ms a held answer could still go out late: deadline now first; (2) only a valid frame dropped a held answer: any received frame now does. Test now also asserts late == 0 and no second answer per thermostat frame, and G5 (held, then a 900 ms stall: nothing sent). A mutant with the old service order fails G5 (late_sent=1). Accepted: A frames of the alternative-slot / UI= / SR= / master paths are still logged at decision time (tagged OTFRAME_SRC_OTDIRECT_LOCAL, not counted in the boiler-support bitmaps); only the relayed path logs on send (C28 contract).

2026-11-11 beta.181 bec10121a, built with beta.182 c9f37bd (esp32-combo, exit 0, artifacts 20:25). evaluate.py --quick 86 pass 0 fail. Bench .61 (OT-Direct, no thermostat, no boiler, otdmode=master): boots, OTA OK, PR=A 'PR: A=OpenTherm Gateway 6.6', PR=T 'PR: T=10', PS=1 one summary line, PS=2 'PS: BV', PS=0 'PS: 0' - identical to beta.178, no regression in the OTD command path. NOT benched: the gate behaviour itself (held answer sent after 50 ms) needs a thermostat on the slave side; .61 has none. Covered only by the host test with the real library gate (tests/test_otd_slave_reply_gate.py, G1-G5).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: otdSendReply/otdReplyService hold an own answer until the library's 50 ms slave gate opens, drop it on the next frame, a mode switch or after 750 ms (deadline first); all six answer sites use it, relayed A logged only when sent. AC2: tests/test_otd_slave_reply_gate.py compiles the real OpenTherm gate and OTDirect code: old code G1-G4 answered 0/150; fix all answered, late=0, dup=0; G5 stall no late send; deadline-after-send mutant fails G5. AC3: esp32-combo build beta.182+c9f37bd (includes beta.181), evaluate 0 FAIL, OTA .61 and PR/PS bench unchanged vs beta.178. Not verified on hardware: the gate path with a real thermostat (no thermostat on any bench unit); evidence is the host test only. Commit bec10121a.
<!-- SECTION:FINAL_SUMMARY:END -->
