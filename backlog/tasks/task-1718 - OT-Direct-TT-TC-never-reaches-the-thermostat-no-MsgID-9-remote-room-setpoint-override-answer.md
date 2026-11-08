---
id: TASK-1718
title: >-
  OT-Direct TT=/TC= never reaches the thermostat: no MsgID 9 (remote room
  setpoint override) answer
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-08T08:13:00Z'
updated_date: '2026-11-08T09:30:00Z'
labels:
  - bug
  - otdirect
  - field-report
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 827000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs 2026-11-08 08:02 #alpha-testing (msg 1568898605186551859): in OT-Direct gateway mode TT=20 'appeared to work but the boiler didn't start'. Cause in code: setRemoteOverride() (OTDirect.ino:2291) only writes MsgID 16 (TrSet) to the boiler plus MsgID 100 flags; nothing answers the thermostat's MsgID 9 read. The PIC does exactly that: gateway.asm:2932-2944 'If a remote override setpoint has been specified, return it to the thermostat' (MessageID9 turns the thermostat's read into an ack carrying setpoint1/2). Without it the thermostat never learns the new setpoint, so it never raises CH demand; MsgID 16 to the boiler does not start heating. Check OT spec v4.2 for MsgID 9 (TrOverride) and the Celcia special case (MsgID 4) before fixing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test (replay): with TT=20 active in gateway mode, the thermostat's MsgID 9 READ is answered with 20.0 (red on old code: forwarded/unanswered)
- [x] #2 TT=0 / thermostat program change clears it as on the PIC (MsgID 100 flags, gateway.asm:3269-3299)
- [ ] #3 Field or bench: thermostat shows the override and raises CH demand (tjfs)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Reproduce first: host replay through the real OT-Direct gateway path (pattern of tests/test_otd_gateway_own_status.py, TASK-1701): TT=20, then a thermostat READ_DATA MsgID 9 -> old code forwards it to the boiler (no 20.0 answer) = red.
Fix (OTDirect.ino, thermostat-request path next to the SR table ~2093, mirrors gateway.asm:2932-2944 MessageID9 and 3269-3299): while otRemoteOverride.mode != NONE, a thermostat READ_DATA for MsgID 9 is answered READ_ACK with otRemoteOverride.f88Value, and a READ_DATA for MsgID 100 with the flag bits (TT 0x0002 / TC 0x0001); after TT=0 (clearRemoteOverride) the next MsgID 9 read is answered once with 0 (PIC OverrideClr), then MsgID 9 goes to the boiler again. Keep the existing MsgID 16 write to the boiler (TrSet, unchanged behaviour); the MsgID 100 write to the boiler is pointless but harmless -- leave it, note it.
Check OpenTherm v4.2 MsgID 9 (TrOverride, R) and 100 (RemoteOverrideFunction, R) semantics before coding; Celcia MsgID 4 special case out of scope (note).
Verification: host test red->green; regression: test_otd_ch_override, test_otd_gateway_own_status, OT-Direct suites; evaluate --quick; bench .61 has no thermostat (monitor/loopback can't prove it) -> AC3 field (tjfs).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 implementation (uncommitted, waiting for the build slot): OTDirect.ino otdRemoteOverrideReply() in replyToThermostat() gateway branch (MsgID 9 -> READ_ACK override f88, MsgID 100 -> READ_ACK flags TT 0x02/TC 0x01; one-shot READ_ACK 0 after clearRemoteOverride; no override + boiler Data-Invalid/Unknown -> READ_ACK 0, PIC parity gateway.asm:2932-2944/3270-3290). Review finding fixed: a boiler that answers MsgID 9 Unknown 3x turned 9 into a TASK-1707 alt slot, so otdAltSlotTaken() answered Unknown and the fix never ran; otdRemoteOverrideHoldsSlot() keeps 9/100 out of the slots while an override is active or its clear is pending (no change without an override, so the alt-slot gates stay as they were). Evidence: tests/test_otd_gateway_alt_slots.py new T1/T1c through the real gateway path (handleSlaveRequest -> boiler model -> replyToThermostat): old code T1 t9=18 ack=0 FAIL, T1c not run (FAIL); fix T1 t9=18 ack=18, T1c first=1 pending=0, Ran 11 OK (skipped=1). tests/test_otd_remote_override_msgid9.py (helper, 10 cases) old FAILED 10 / fix OK. OT-Direct suites OK; evaluate --quick Passed 86 Failed 0. Out of scope, unchanged: TT= still also writes MsgID 16/100 to the boiler (MsgID 100 is R-only per spec; harmless). Open question for Robert: answer MsgID 9 with READ_ACK 0 also when no override is active (full PIC parity, costs alt slots) - not done.

2026-11-08 pushed 4f7fd7742 as beta.142, zip posted #alpha-testing 1568920313243377674, tjfs asked for the field check (1568920439214973040). Bench .61 (OT-Direct mode 3, no thermostat): OTA app+fs, settings 164 changed none, bootcount 146->148, no crashlog; POST /api/v2/otgw/commands TT=20 -> 202 queued; TT=0 -> 202; /api/v2/otdirect/overrides afterwards lists only MsgID 1 and 14 write overrides (no 16/100), so TT is set and cleared. Full suite: test/host/test_override_reply suite 1178 broke on the new replyToThermostat dependency (harness slice lacked otRemoteOverride) and C8b encoded the old pass-through of a boiler Unknown on MsgID 100; harness slice extended and C8b now expects the gateway's READ-ACK 0 (PIC MessageID100 parity): 4/4 OK. Test-only follow-up commit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: tests/test_otd_gateway_alt_slots.py T1 through the real gateway path: old code 0 of 18 MsgID 9 reads during TT=20 answered 20.0, fix 18 of 18; helper test test_otd_remote_override_msgid9 old 10 FAIL, fix OK. AC2: T1c: first MsgID 9 read after the clear is READ_ACK 0, flag cleared; MsgID 100 flags TT 0x02/TC 0x01 covered by the helper test. Gates: evaluate --quick 86/0; OT-Direct suites OK; test_override_reply 4/4 after the harness update. Bench .61 beta.142+4f7fd77: TT=20/TT=0 accepted and cleared, no restart. AC3 (thermostat shows the override and the boiler starts) needs tjfs's field check: In Review.
<!-- SECTION:FINAL_SUMMARY:END -->
