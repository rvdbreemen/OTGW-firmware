---
id: TASK-1805
title: >-
  OT-Direct gateway mode rewrites MsgID 0 bits 1, 2, 4, 5 and 6 like the PIC
  (HW=, SM=, BW=, CE=, H2=); new ADR superseding ADR-208
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:42:00Z'
updated_date: '2026-11-12T00:07:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - feature
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 898000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report A, A2 (B1). Robert's decision (2026-11-11 ~07:45, AskUserQuestion in the coordinator session, relayed): 'Like the PIC: new ADR'. Today HW=, SM=, BW=, CE= and H2= change only otMasterStatusFlags (OTDirect.ino:3044-3138), which only buildStatusRequest reads (:866-879; call sites :995 boot probe and the R_ENTRY(0) schedule :1761). With a thermostat in gateway mode otdOwnFramesAllowed (:861) blocks that schedule and the alternative slots skip MsgID 0 (:1831), so the relayed frame gets only bit 0 (otdRelayedStatusFrame :837-845, ADR-208) and these commands are acknowledged but do nothing. The PIC rewrites bits 1, 2, 4, 5 and 6 of the thermostat's own MsgID 0 (readstatus gateway.asm:2712-2740). ADR-208 forbids rewriting bits other than bit 0 and rests on the gateway's own MsgID 0 schedule, which TASK-1701 removed (otdOwnFramesAllowed :861; ADR-224 confirms), so the ADR's premise no longer holds. Scenario: an HA automation sends HW=0 to block DHW; OT-Direct answers 'HW: 0' and the boiler keeps seeing the thermostat's DHW enable. ADR number reserved by the coordinator: ADR-229 (next ADR numbers only through the coordinator).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A new ADR made with /adr-kit:adr supersedes ADR-208: Proposed, with Robert's choice 'Like the PIC: new ADR' as the User Decision; never self-accepted
- [x] #2 The relayed thermostat MsgID 0 carries the HW=/SM=/BW=/CE=/H2= bits (1, 2, 4, 5, 6) by the PIC readstatus rule (gateway.asm:2712-2740), bit 0 stays as ADR-208
- [x] #3 Host tests against the real relay code (otdRelayedStatusFrame and its callers): red on the old code, green on the fix, one case per bit
- [x] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 d3: ADR-229 created with adr-kit (python bin/adr new; number reserved by the coordinator), Proposed, User Decision 'Like the PIC: new ADR'; supersedes ADR-208 on acceptance (adr supersede then; ADR-208 untouched now, so the bidirectional-supersession lint stays clean). adr-lint: no FAIL for ADR-229; tests/test_adr_governance.py 17 passed (README entry added, index regenerated). PIC rule decoded from gateway.asm:2712-2742 (readstatus): bit 1 mask HotWaterSwitch||dhwpushspoof value HotWaterEnable||push; bit 2 mask CL= level held (SysCoolLevel = coolinglevel1 bit 7 clear) value !CoolModeOff (CE=); bit 4 mask C2= setpoint held (SysCH2Setpoint) value !CH2ModeOff (H2=); bit 5 SM=1 forces 1; bit 6 BW=1 forces 1; bits 3, 7 pass. Implementation: OTDirect.ino otdRelayedStatus block: OtdStatusCmd + pure otdRelayedStatusHB() + otdWriteOverrideActive(); otdRelayedStatusFrame() fills it from otDHWOverride/otDHWPushState, the MsgID 7/8 write overrides, otCoolingEnable, otMasterStatusFlags bit 4, otSummerMode, otDHWBlocking (otDHWBlocking/otCoolingEnable declarations moved up). Bit 0 unchanged (ADR-208 rule).

2026-11-11 tests: new tests/test_otd_status_bits.py compiles the real setOTParityBit, otdEffectiveMasterFlags and otdRelayedStatus block and drives otdRelayedStatusFrame() (the applyOverrides entry point): pass-through of all 256 high bytes, one case per bit with mask on and off (HW 0/1/A/push, CE with/without CL, H2 with/without C2, SM, BW), bit 0 = ADR-208, bits 3/7 untouched, all together; parity checked. On origin/dev OTDirect.ino (OTGW_OTD_INO): FAILED (failures=7), only the pass-through case passes; on the fix: 8 OK. tests/test_otd_status_relay.py (ADR-208 cases) stub preamble extended with the new globals: OK; test_otd_gateway_alt_slots 12 OK (1 skipped), test_otd_ch_override OK, test_sat_requested_vs_transmitted OK. evaluate.py --quick 86/0.

2026-11-11 full scripts/run_tests.py: 201 of 203 passed. test/host/test_override_reply.py failed to compile the fix slice (otSummerMode, otDHWBlocking, otCoolingEnable undeclared: the harness slices OTDirect.ino by anchor and these globals are outside its slices). Slicing them breaks its old-revision suites (anchors must match old revisions), so test_override_reply.cpp declares them before the slice; suites 1177/1178/1184/1684 OK. test/host/test_stream_real_cbuf.py fails only under -j 4 and passes alone (exit 0), unrelated (seen on the TASK-1813 run too).

ADR-229 Accepted 2026-11-11 by Robert van den Breemen via the coordinator session ('Accept both (Recommended)', AskUserQuestion answer in session 9604259c at 19:10Z, verified in that transcript). Steps: adr answer (open question), adr accept --confirm, adr supersede ADR-208 --by ADR-229. Clarity gate first failed (PIC/DHW/MQTT without expansion); fixed inline. Full-dir strict lint: ADR-208 and ADR-229 PASS strictly; tests/test_adr_governance.py 17 OK.

Bench on .61 (OT-Direct, beta.184+d7b32cb), what is measurable without a thermostat: the build boots on OT-Direct (combo image, otcommandinterface OT-Direct); HW=0, SM=1, BW=1, CE=1, H2=1 are accepted (HTTP 202 queued) and undone (HW=A, SM=0, BW=0, CE=0, H2=0), health stays UP (heap 69 KB). NOT benched: the rewrite of a relayed thermostat MsgID 0 in gateway mode, because there is no thermostat on any bench unit in OT-Direct gateway mode; that path is proven by tests/test_otd_status_bits.py (8 OK on the fix, 7 failures on the old code) and test_otd_status_relay.py (9 OK). Field check to be asked from tjfs in the release note (via 92). A first combo build failed: OtdStatusCmd defined in OTDirect.ino is named by the Arduino-generated prototypes; moved to OTDirecttypes.h (bug-8668 in .wolf/buglog.json).

PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (Done): each left a gap; filed as N3 and N12 instead of reopening. The gap is filed as TASK-1833 (N3: CE=/H2= default on, CL=0 holds 0 %).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: ADR-229 via adr-kit, Accepted 2026-11-11 by Robert (coordinator session, 'Accept both'), supersedes ADR-208. AC2: otdRelayedStatusFrame()/otdRelayedStatusHB() apply the PIC readstatus rule (gateway.asm:2712-2742) to bits 1, 2, 4, 5, 6; bit 0 as ADR-208. AC3: tests/test_otd_status_bits.py 8 OK on the fix, 7 failures on the old code; test_otd_status_relay.py 9 OK; override_reply 4 OK; the OTD helper suites green after OtdStatusCmd moved to OTDirecttypes.h. AC4: esp32-combo built (beta.184, posted 893c5b7, msg 1570199829844590718), evaluate --quick 86/0; bench .61: boots on OT-Direct, HW=0/SM=1/BW=1/CE=1/H2=1 accepted and undone, health UP. NOT benched: the rewrite of a relayed thermostat MsgID 0 in gateway mode (no thermostat in OT-Direct gateway mode on any bench unit); field check to be asked from tjfs via 92.
<!-- SECTION:FINAL_SUMMARY:END -->
