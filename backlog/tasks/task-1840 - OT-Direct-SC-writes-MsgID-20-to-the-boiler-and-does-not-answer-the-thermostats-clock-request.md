---
id: TASK-1840
title: >-
  OT-Direct: SC= writes MsgID 20 to the boiler and does not answer the
  thermostat's clock request
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:51:00Z'
updated_date: '2026-11-12T04:44:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 921000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N8), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: SC= only stores the time (G:4331-4388); used to answer the thermostat's MsgID 20 READ/WRITE for one expiry window (MessageID20 G:3048-3056); after that a boiler Unknown/Data-Invalid becomes Data-Invalid for READ, WRITE_ACK for WRITE (G:2694-2706). tests/thermostat-2.stc: A50147532, AC0147532, later A5014892A, A60140000. Nothing written to the boiler.
- OT-Direct: enqueueWriteCommand(20,...) WRITEs the boiler (O:3181-3191); its "override" does not exist (O:2433-2437); the thermostat gets the boiler's answer (O:1548-1582).
- Consequence: with "send time" on the firmware sends SC= every minute (networkStuff.ino:1447): boiler clock written every minute, thermostat never sees the time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SC= puts no frame on the bus; host test red on current code
- [x] #2 while SC valid, thermostat READ/WRITE of MsgID 20 answered with the SC value; after expiry Data-Invalid/WRITE_ACK as the PIC, also once blacklisted
- [ ] #3 bench capture with a thermostat that reads MsgID 20, or the slave simulator
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): expiry window length (see N4).

Part of the PIC 6.8 parity audit, parent TASK-1851.

Implementation (wt-95fw, lane-95fw from origin/dev ba41264e6): SC= now only stores otSCClockData (HB = dow<<5 | hours, LB = minutes) and stamps otSCLastCommandMs; no enqueueWriteCommand(20). New pure helper otdClockReply() (markers otdClockReply begin/end), PIC MessageID20/missingclock (gateway.asm:3048-3056, :2694-2706, messageack :2676-2679, messageinv :2674-2675): valid time -> matching ACK with the SC time; no valid time -> boiler Data-Invalid/Unknown becomes Data-Invalid (READ, reply data kept) or WRITE_ACK with the thermostat's own data (WRITE); any other boiler answer passes. Applied in replyToThermostat() (gateway mode), in otdAltSlotTaken() and in the UI= table branch (blacklisted MsgID 20, as TreatMessage runs MessageID20 on the fake Unknown-DataID, gateway.asm:2467-2477). The expiry now invalidates the time (the old clearWriteOverride(20) was a no-op: MsgID 20 has no override slot). OT_SC_EXPIRY_MS 61000 -> 63500: the PIC keeps it at least 121 x 524.288 ms = 63.4 s (gateway.asm:157-158; tests/thermostat-2.stc: SC= at 2 s still answered at 66 s). The upper end of the PIC window (up to ~126 s) is not copied; that choice stays with TASK-1834 (N4). Manual ch10 SC= row corrected (it said 'Synchronize boiler clock').

Evidence AC1+AC2: tests/test_otd_sc_clock.py (8 tests, real SC= branch with a recording enqueueWriteCommand, real otdClockReply with the real setOTParityBit/buildOTResponse, thermostat frames from tests/clock.stc). Fix: 8/8 OK. Old code (origin/dev ba41264e6 via OTGW_OTD_INO): 8/8 FAIL; the behavioural one shows the bug: 'enq=1 id=20 data=7532' (SC=21:50/3 enqueued WRITE 20 to the boiler). Expected frames match tests/thermostat-2.stc: valid 50147532 / C0147532, expired 5014892A / 60140000.

Harnesses that slice the touched code, kept green in the same lane: tests/test_otd_mode_switch_queue_reset.py (takes the SC state + otdClockReply when present; 8 OK) and test/host/build_and_run_override_reply.ps1 (optional anchor 'SC= clock answer'; test/host/test_override_reply.py 4 OK). Also run and green: test_otd_gateway_alt_slots (12), test_otd_return_temp_slot, test_otd_slave_reply_gate, test_otd_tt_pic_parity, test_otd_ps1_summary_pic, test_sat_command_delivery (22), test_sat_lowload_contract (exit 0, 523.8 s, missing LL ids: none). python evaluate.py --quick: exit 0, 92 checks, 0 failed, 0 warnings. A full scripts/run_tests.py sweep was stopped by the harness for low memory (not by a failure); per the coordinator a full run now goes through the coordinator one at a time.

Open: AC3 bench capture, via the bench-tester (TASK-1826 rule): SC= on the OT-Direct unit with a thermostat or the slave simulator reading MsgID 20; expect no R..014.... WRITE to the boiler and A frames as above. Not run: esp32-combo build (build slot via the coordinator).

Bench checklist for otgw-bench-tester (TASK-1826 rule; the bench-tester chooses and ticks). On .61 (OTGW32) in LOOPBACK mode (GW=L): loopback logs the gateway's own requests as R frames (OTDirect.ino bridgeFrameToParser 'R'), so the boiler side is observable: (1) send SC=21:50/3; OLD code shows an R frame WRITE MsgID 20 with data 0x7532 (R..147532) within seconds; FIX shows no R..014 frame, and the reply is 'SC: 21:50/3'. That is the only part .61 can show without a thermostat. NOT observable in loopback: the thermostat answer (otdClockReply runs only in gateway mode, on a thermostat frame). It needs a thermostat or an OT slave simulator on the thermostat port of a gateway-mode unit: thermostat READ 0x00140000 -> A frame C0147532 while SC is valid, A frame 60140000 (Data-Invalid) about 64 s after the last SC=, WRITE 0x9014892A -> 50147532 / 5014892A. Without such a setup AC3 stays open for that part; the answer path is covered by the host test only (do not tick it on air).

2026-11-12 bench (otgw-bench-tester), .61, old vs fix in loopback over TCP 25238. OLD 2.0.0-beta.200+f1bb2a0: SC=21:50/3 replied 'SC: 21:50/3', then R90147532 (WRITE MsgID 20, 0x7532 = Wednesday 21:50), answered by B50147532; telnet 'enqueueWrite: OTD: SC-time -> MsgID 20 frame 0x90147532'. NEW 2.0.0-beta.203+61b69e0: reply 'SC: 21:50/3' and NO R/B frame for MsgID 20. This is bench evidence for the 'no frame on the bus' behaviour (AC1). AC3 NOT ticked: it asks for a thermostat that reads MsgID 20, or the slave simulator; neither was used. Evidence: scratchpad b203/old1840-out.txt, check203_12-out.txt.
<!-- SECTION:NOTES:END -->
