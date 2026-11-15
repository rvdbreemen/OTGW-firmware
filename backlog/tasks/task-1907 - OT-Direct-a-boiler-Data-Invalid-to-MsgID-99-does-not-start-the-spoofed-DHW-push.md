---
id: TASK-1907
title: >-
  OT-Direct: a boiler Data-Invalid to MsgID 99 does not start the spoofed DHW
  push
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T18:50:00Z'
updated_date: '2026-11-15T09:09:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 973000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1851 delta parity check (2026-11-13, read-only, verified in both sources). PIC 6.8: in MessageID99's type jump table the Data-Invalid entry is a nop that falls through into ID99Unknown (gateway.asm:3218-3230), so a Data-Invalid answer is handled like Unknown-DataID: while a manual push is set (manualdhwpush) the gateway sets dhwpushspoof and forces DHW enable. OT-Direct: otdPushOnBoiler99() is only called in the success branch of handleMasterResponse (OTDirect.ino:1865); the boilerDataInvalid branch only bridges the frame to the parser (:1875-1877), and otdPushOnBoiler99 also returns for type 6 (:2685-2687). Effect after HW=P against a boiler that answers MsgID 99 with Data-Invalid: the push stays pending for good, DHW is never enabled, a new HW=P does nothing, and MH/MW/M2 keep sending the push bit. Basis: code reading; no PIC .stc test covers it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real handleMasterResponse and otdPushOnBoiler99 code: HW=P, then a boiler Data-Invalid to the WRITE 99 gives otPushSpoof = true and bit 1 set in the relayed MsgID 0; red on the current code, green on the fix
- [ ] #2 Bench or replay: HW=P against a Data-Invalid MsgID 99 answer enables DHW once, as the PIC does
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Scope OTDirect.ino only. (1) otdPushOnBoiler99: handle type 6 (DATA_INVALID) like type 7: if a manual push is set, spoof it (PIC MessageID99: the Data-Invalid jump entry is a nop that falls into ID99Unknown, gateway.asm:3227-3230; dhwpushpending is not cleared there, so otPushPending is left as is too). (2) handleMasterResponse: the boilerDataInvalid branch hands a MsgID 99 reply to otdPushOnBoiler99(6, data). Proof: tests/test_otd_dhw_push_pic.py (real push block): HW=P then b99:6 -> relayed MsgID 0 HB 02 (DHW enable forced by the spoof), no push asked -> 00; plus a source pin on the Data-Invalid branch of the real handleMasterResponse. Red on the old code, green on the fix.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 d3, lane-1907 (wt-1907, uncommitted until a bump slot after 95's 230). Fix applied as planned. Evidence: tests/test_otd_dhw_push_pic.py on the old code 2 FAIL (test_dhw_enable_forced_when_the_boiler_refuses_or_lacks_the_push: b99:6 gave HB 00; test_handle_master_response_feeds_a_data_invalid_99_to_the_push: no call in the branch), on the fix OK. Related OTD tests OK (gateway_alt_slots, loopback_not_boiler_for_sat, msgid99_bytes, tt_pic_parity, write_once). evaluate --quick 85/0. AC1 note: the handleMasterResponse part is a source pin on the real branch (the push harness does not cut handleMasterResponse); the behaviour part runs the real otdPushOnBoiler99 + relayed-status code.

2026-11-14 shipped in 2.0.0-beta.232 (9c3224077, post 1570967740544450631). Bench on .61 (09): no regression (HW=P in loopback as before, health OK). AC2 not bench-testable: the OT-Direct loopback answers go through simulateLoopbackResponse/bridgeFrameToParser and never reach the boilerDataInvalid branch of handleMasterResponse, and a test-only loopback hook would not exercise that branch either. Evidence for AC1: tests/test_otd_dhw_push_pic.py (old 2 FAIL, fix OK) plus the source pin on the real branch. In Review for field confirmation (a boiler that answers MsgID 99 with Data-Invalid).

AC 2: field AC (Robert C, 2026-11-14, DM 1571276554430976183 via the coordinator, no OT bus on the bench): HW=P against a Data-Invalid MsgID 99 answer is verified from a tester capture, not on the bench. Status stays In Review until a tester capture covers it.

2026-11-15: closed as field AC on Robert's A (DM 1571431149795024929) after 09's bench check (no bench route).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed on Robert's decision A to triage question 1/5 (bot DM 1571283730117562467; his answer '1. A' in DM 1571431149795024929, 2026-11-15). Before closing, the bench-tester (09) checked whether the open AC can be done on the bench (2026-11-15, from the code at origin/dev 6d6f5b71d): no. The push follows the boiler's answers only in handleMasterResponse() (OTDirect.ino:1913; otdPushOnBoiler99 at :1961 for a SUCCESS answer to 99, :1976 for a genuine DATA_INVALID), which runs only on a real bus answer (:2539). Loopback answers itself in sendMasterRequestAsync() (:1827-1840) before that function and echoes every WRITE as WRITE_ACK (:1653-1656); a replay feeds processOT(), not the OT-Direct master state machine (OTGW-Core.ino:6462). A bench route would need a boiler emulator (TASK-1654) or a second gateway as slave. Open field AC #2 (HW=P against a Data-Invalid MsgID 99 answer): a Data-Invalid can only reach :1976 from the bus; the host test of AC1 covers the real code.
<!-- SECTION:FINAL_SUMMARY:END -->
