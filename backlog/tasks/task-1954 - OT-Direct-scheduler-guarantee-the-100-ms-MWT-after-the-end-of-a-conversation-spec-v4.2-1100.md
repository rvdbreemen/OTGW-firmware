---
id: TASK-1954
title: >-
  OT-Direct scheduler: guarantee the 100 ms MWT after the end of a conversation
  (spec v4.2 :1100)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T02:12:00Z'
updated_date: '2026-11-15T14:20:00Z'
labels:
  - otdirect
  - spec
dependencies: []
priority: low
ordinal: 1000000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Spin-off of the ADR-241 review (92, d3, 2026-11-15). OpenTherm spec v4.2 :1100: the master waits 100 ms (MWT) from the END of the previous conversation before it starts a new one. scheduleMasterRequest() gaps the gateway's own frames by otMinIntervalMs (MI=, default 100 ms) counted from otLastAnySendMs, the START of the previous send, and waits for !otMasterRequestActive. With a late boiler answer, send start + MI can fall less than 100 ms after the answer, so an own frame can start inside the MWT. The OpenTherm library only enforces RESPONSE_TIME (50 ms) after a received frame (OpenTherm.cpp:443). The TASK-1812 relay (ADR-241, not merged yet) already waits OTD_MWT_MS from otdLastConvEndMs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real scheduleMasterRequest() with fake time: with a boiler answer late enough that send start + MI ends < 100 ms after the answer, the next own frame starts at least 100 ms after the answer; red on the current code, green on the fix
- [x] #2 MI= keeps its meaning as a minimum gap between sends; the MWT is an additional floor
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (d3, lane-1954 from c2cd68b16): OTD_MWT_MS = 100 and otdLastConvEndMs next to otLastAnySendMs; handleMasterResponse() stamps otdLastConvEndMs unconditionally right before otMasterRequestActive = false (answer, invalid frame, timeout); scheduleMasterRequest() returns while now - otdLastConvEndMs < OTD_MWT_MS, after the MI= check (MI= keeps its meaning, the MWT is an extra floor, AC2). Not gated: gateway-mode thermostat relays and alternative slots (the thermostat sets that timing) and loopback (no bus). Same names as the TASK-1812 relay on lane-1924-1812, which will reuse them at its rebase.

Evidence AC1: new tests/test_otd_mwt.py (real scheduleMasterRequest() with the test_otd_offline_retry harness, fake time and bus, runs on old and new code): fix 'mingap=100' for a 900 ms and a 30 ms answer; old (HEAD c2cd68b16 copy) FAILED: 'mingap=1' (900 ms answer) and 'mingap=70' (30 ms answer); plus a source check that handleMasterResponse stamps the end unconditionally. Updated on purpose: test_otd_gateway_alt_slots S1 threshold 300 -> 250 MsgID 0 frames (its 40 ms boiler now gives 140 ms per own frame instead of 100 ms; measured 297); MWT stubs in test_otd_mode_switch_queue_reset (shared prelude), test_otd_gateway_own_status, test_sat_no_return_startup; host-slice anchor in test/host/build_and_run_override_reply.ps1. All tests/test_otd*.py OK, test/host/test_override_reply.py OK, evaluate.py --quick 0 failed. C4 doc updated. Effect on a real bus: own frames at most one per (answer time + 100 ms); with typical 30-100 ms answers that is 130-200 ms instead of 100 ms. Build + bench open.

Review by 92 handled: (1) a TIMEOUT no longer stamps the end of the conversation: the bus has been quiet ~1 s since our frame, and the MWT on top would push an offline MsgID 0 past 1.15 s (spec v4.2 :1101); new case test_a_timeout_keeps_the_next_frame_within_the_master_interval: maxstartgap 1001 ms (<= 1150); test_otd_offline_retry still 'dead maxgap=1010'. (2) Gateway alternative slots are NOT gated: otdAltSlotTaken() sends through sendMasterRequestAsync() directly, and scheduleMasterRequest() already returns in gateway mode with a thermostat (otdOwnFramesAllowed), so a thermostat faster than its own MWT keeps its alt slot; checked in the source, no behaviour change there. (3) test_otd_gateway_alt_slots S1 is now a band 270 < MsgID 0 frames < 330 over its 360 s run (measured 297), with a reference to test_otd_mwt. (4) Merge with lane-1924-1812 (TASK-1812): 1954 lands first and owns OTD_MWT_MS and otdLastConvEndMs; 1812 rebases onto it and reuses them (drops its own definitions and its own stamp).

2026-11-15 Bench 09 on .61, master mode without a boiler, 5 min each: beta.297+85bc0dc (1954 + bug 9): 270 frames, gaps min 1.038 / avg 1.116 / max 1.171 s, 1 of 269 > 1150 ms. Reference beta.301 (pre-1954, 95's build): 268 frames, avg 1.117, max 1.229 s, 1 > 1150 ms. So 1954 does not change the no-boiler cadence: without a boiler every request is a TIMEOUT, which does not stamp otdLastConvEndMs (OTDirect.ino handleMasterResponse), and the rare > 1150 ms gap is the old path of the library's 1 s timeout + loop latency, present before 1954 and only without a boiler. Host test test_otd_mwt: max start-to-start gap 120..1090 ms at answer delays 20..990 ms. Coordinator decision: no follow-up task; the 1954 bench AC passes with this reference as evidence.

2026-11-15 Added in this lane (coordinator decision B): TASK-1804 Bug 4, MsgID 0 goes first whenever it is due (scheduleMasterRequest). Reason: the 1804 C/D triage found that 1954's MWT is ~150 ms in practice (handleMasterResponse runs after isReady(), which the library holds 50 ms after the stop bit, OpenTherm.cpp:443), so every own frame got ~50 ms longer and the round-robin held MsgID 0 back longer. New tests/test_otd_status_priority.py (real scheduleMasterRequest, 105-entry table shape, 50 ms boiler + 50 ms library hold + MWT): max MsgID 0 gap 10 s tier / 60 s burst: origin 2000 / 10100 ms, this lane before the fix 2150 / 12700 ms, fix 900 / 900 ms. test_otd_gateway_alt_slots S1 band updated (MsgID 0 now 450 in 360 s, was 297). All 54 test_otd_*.py green, evaluate 86/0. The MWT refinement (count from the stop bit) stays in TASK-1804, after 95's OTDMasterLine (TASK-1966) is on origin. Final build gets a short bench (coordinator).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pushed 2026-11-15 as beta.307 (origin/dev 22f4fece5; src identical to the benched 2.0.0-beta.309+ada14c6). AC1: tests/test_otd_mwt.py on the real scheduleMasterRequest(): old min gap 1 ms (900 ms answer) / 70 ms (30 ms answer), fix >= 100 ms; max start-to-start gap 120..1090 ms at answer delays 20..990 ms. AC2: MI= stays a gap from the start of the previous send, the MWT is an extra floor from its end (timeout does not stamp). Added in this lane (coordinator B): TASK-1804 Bug 4, MsgID 0 first when due: tests/test_otd_status_priority.py max MsgID 0 gap origin 2000/10100 ms -> 900/900 ms. Bench 09 on .61: beta.297 avg 1.116 / max 1.171 s; 301 reference avg 1.117 / max 1.229 s (the > 1150 ms gap is the old no-boiler timeout path); final 309 avg 1.120 / max 1.145 s, 0 > 1150 ms. Not benchable: the 100 ms after a real boiler answer (no boiler on the bench). Open follow-up in TASK-1804: count the MWT from the stop bit (the library's 50 ms hold makes it ~150 ms in practice).
<!-- SECTION:FINAL_SUMMARY:END -->
