---
id: TASK-1834
title: >-
  OT-Direct: a CS=/C2= override expires after 60 s; the PIC keeps it at least
  62.9 s
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-11T23:43:00Z'
updated_date: '2026-11-14T03:02:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 917000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N4), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: expiry counts 121 Timer-1 overflows of 524.288 ms (G:157-158, :1104-1112, StartTimer G:4233-4239, CommandExpiry G:1623-1646): lives 62.9 to ~126 s after last command. tests/control-3.stc: CS at 2 s still active at 32 s, gone between 65 and 74 s. Command ref "repeated at least every minute".
- OT-Direct: OT_CSC2_EXPIRY_MS = 60000, checked once a second (O:499-501, :2421-2431).
- Consequence: a client refreshing exactly every 60 s never lapses on the PIC; on OT-Direct the override can drop just before the refresh so the thermostat TSet reaches the boiler briefly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test with fake clock on real expiry code: last CS= at t=0 still active at t=62 s; red on current code
- [x] #2 expiry not earlier than 63 s for CS and C2
- [x] #3 bench: CS= every 60 s for 15 min, no thermostat-valued MsgID 1 in the capture
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PIC window derived this session from gateway.asm 6.8: Timer 1 period 524.288 ms (:157-158); StartTimer loads minutetimer = 120 and clears the heartbeat flags (:4233-4239); CommandExpiry decrements on every overflow and acts when bit 7 (Expired, :368) sets, i.e. on the 121st tick (:1623-1646, called from FlashTimer :1612-1614); SetCtrlSetpoint sets HeartbeatCS then calls StartTimer (:4227-4228), so the first CS= of an idle timer has its flag cleared at once. Bounds: a single CS=/C2= lives 120..121 ticks = 62.9..63.44 s (tick phase); a refresh inside a running window keeps it to the end of the next window, up to ~126.9 s. tests/control-3.stc agrees (CS at 2 s active at 32 s, gone between 65 and 74 s). Chosen value: a fixed 63500 ms from the last command (OT_CSC2_EXPIRY_MS), the same as OT_SC_EXPIRY_MS (TASK-1840), checked once a second: never shorter than the PIC's minimum, so a client refreshing every 60 s never lapses. Deliberate simplification: the PIC's window model can keep an override up to ~126.9 s for a client that refreshes late; not emulated (KISS; the command reference asks for a refresh at least every minute). SAT: unchanged. The CS expiry is skipped while satOwnsControlSetpoint() (OTDirect.ino 1-second status timer, TASK-1150), so SAT's own CS (satEnqueueOwnCS, 5 s resend ADR-203/207) never expired before and still does not; C2 is not used by SAT.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): copy the full 63-126 s window or only the lower bound (Robert's call). NOTE: interacts with TASK-1811 and with SAT's 5 s CS resend (ADR-203/207, SAT exempt from CS expiry per TASK-1150).

Part of the PIC 6.8 parity audit, parent TASK-1851.

2026-11-14 d3, lane-1834 (wt-1834, uncommitted until a bump slot). Fix: OT_CSC2_EXPIRY_MS 60000 -> 63500 with the PIC citation. Evidence: new tests/test_otd_csc2_expiry.py (real constant + the real CS/C2 expiry statements of the 1-second timer, fake clock, CS= and C2= at t=0): old code FAIL at 61/62 s and at 63 s; fix: active at 61, 62, 63 s, expired at 64 and 70 s (CS and C2), SAT-owned CS still active at 150 s. All 43 tests that read OTDirect.ino OK, test_sat_lowload_contract OK (35, 495 s), evaluate --quick 85/0. Not in scope, noted: the PIC also keeps a CS below 8 C forever (CommandExpiry :1628-1633, review finding b1-7); OT-Direct still expires it after the window.

2026-11-14 shipped in 2.0.0-beta.234 (08f42d625, #alpha-testing post 1570996739945537607, zip sha256 49eb9bbb83f28d7cdb7ced481b322e041c41e23eb23331a1fef300c456da409c). Bench by 09 on .61 (evidence 09 scratchpad b234): A: 16 x CS=45.5 in 15 min, 0 expiries, MsgID 1 only R90012D80; B: expiry 63.53 s after the last CS=; C2: expiry 63.73 s. Named limit for AC3: .61 runs in master mode without a thermostat, so the gateway-mode substitution against a real thermostat TSet was not exercised; the timer is proven, the substitution path is unchanged code. AC3 left for 09 to tick or waive (bench ACs are ticked by the bench tester).

Bench AC3 by otgw-bench-tester (09), .61 OT-Direct otdmode 3 (master), SAT off, sim off, 2.0.0-beta.234+08f42d6 (OTA, bootcount 621->623, crashlog empty), 2026-11-14, evidence 09 scratchpad b234\ (61-telnet.txt, events.log). Test A 03:40:58-03:55:58: 16x CS=45.5 over REST every 60 s; telnet 16x 'OTD: cmd "CS=45.5"', 0 'CS heartbeat expired' lines in the window; every MsgID 1 write in the window is R90012D80 (TSet 45.50), 16 frames, no other value. Test B: last CS= on the device 03:55:56.924, 'OTD: CS heartbeat expired, clearing MsgID 1 override' 03:57:00.457 = 63.53 s (device clock). C2: C2=40 at 03:57:15.861, MsgID 8 written once (R10082800), 'OTD: C2 heartbeat expired, clearing MsgID 8 override' 03:58:19.587 = 63.73 s. Cleanup CS=0 + C2=0, settings diff none. Limit: .61 has no thermostat (master mode), so 'no thermostat-valued MsgID 1' holds trivially; in this mode MsgID 1 is written once per CS= command (~1.2 s after it), not on a schedule. The bench proves the 60 s refresh never lapses and the 63.5 s expiry, not the gateway-mode substitution against a real thermostat TSet.
<!-- SECTION:NOTES:END -->
