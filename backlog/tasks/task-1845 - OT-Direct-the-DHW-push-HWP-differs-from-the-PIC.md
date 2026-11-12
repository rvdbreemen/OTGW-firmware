---
id: TASK-1845
title: 'OT-Direct: the DHW push (HW=P) differs from the PIC'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:57:00Z'
updated_date: '2026-11-12T01:58:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 924000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N11), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: HW=P leaves stored HW state alone (G:4410-4412); any other HW= aborts the push (G:4398-4399); DHW enable forced only when the boiler does not take the push (MsgID 99 unsupported/Unknown/acked without push bit, G:3229-3238, :3273-3279); once started the push ends when the boiler's DHW mode bit clears, before that 8 status answers with flame off end it (G:2783-2797). tests/dhw-5.stc: with boiler support and HW=0 status stays R00000000.
- OT-Direct: HW=P sets otDHWOverride=1 (O:3202-3203); push end sets it to auto (O:1733, :1745) -> a previous HW=0 is gone; DHW enable always forced during a push (O:910-915); 30 s wall-clock timeout also cuts a started push (O:1728-1735); only HW=0 aborts (O:3218-3225); thermostat's own MsgID 99 not handled.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HW=0, HW=P, push ends: PR=W answers W=0 during and after, bit 1 = 0 afterwards; host test red on current code
- [x] #2 a started push is not cut at 30 s
- [x] #3 DHW enable forced only when the boiler refuses or lacks the push
- [ ] #4 bench capture of one push
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): on the PIC MH=/MW=/M2= also set dhwpushpending (PushOperModes G:3281-3288), which looks like it can start a spoofed push after a plain mode command; read from code only, check before copying. Depends on N2.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Ground truth read in this session (gateway.asm 6.8): SetHotWater 4392-4414 (HW=P leaves HotWaterSwitch/Enable alone, other HW= aborts the push), StartDHWPush/PushOperModes 3270-3288, MessageID99 3218-3262 (Unknown-DataID during a push or WRITE_ACK without the push bit to the request -> dhwpushspoof; otherwise OperModeFlag copies the boiler's push bit), readstatus 2720-2747 (DHW enable forced only by HotWaterSwitch or dhwpushspoof), statusreturn/dhwpushcheck 2753-2797 (only while spoofing; flame+DHW starts, 8 flame-off answers before the start end it, DHW mode off ends a started push; no timeout); tests dhw-5.stc (R10631000, status stays R00000000 with boiler support) and dhw-6.stc (spoof in eco mode).

Fix: PIC-named flags otPushManual/Pending/Spoof/Started/Fails replace the old enum and 30 s timeout; otdPushStart/End/OnBoiler99/OnBoilerStatus (block '// --- otdPush'); handleMasterResponse feeds MsgID 99 answers and MsgID 0 READ_ACKs to them; HW=P no longer touches otDHWOverride/otMasterStatusFlags; any other HW= ends the push; relayed status and the gateway's own status force DHW enable only while spoofing; MH/MW/M2 keep the push bit while a push runs. Not copied: the PIC's MH/MW/M2 setting dhwpushpending (the audit flagged it as unverified) and the thermostat's own MsgID 99 handling (no AC). Coordinated with 95 (TASK-1836 adds otdPersistHW in the HW branch; second lander rebases).

Tests: new tests/test_otd_dhw_push_pic.py (real HW branch, otdOperModeData, otdPush block, otOverrides, otdEffectiveMasterFlags/otdRelayedStatus, OtdStatusCmd): old code 6 FAIL, fix 6 OK (HW=0 + HW=P -> SENT 10631000 = dhw-5.stc, W=0 during and after, bit 1 = 0; DHW enable not forced when the boiler takes the push; forced on WRITE_ACK without push bit, Unknown-DataID, or 99 known unsupported (no frame then); a started spoofed push survives 60 s and ends when DHW mode clears; 8 flame-off answers end it; HW=A aborts). Limitation: the old 30 s cut lived inside handleMasterResponse, not cut into this harness; on the old code that test is red because the old answer path is stubbed, so the 30 s red is shown by reading the old code (OT_DHW_PUSH_TIMEOUT_MS = 30000 checked in handleMasterResponse). Harness updates for the new flags: status_bits, status_relay, cool_ch2_defaults, mode_switch_queue_reset, msgid99_bytes, gateway_own_status, gateway_alt_slots (slices now include the push block; tt_pic_parity and ps1_summary use it). All OT-Direct suites OK; evaluate --quick 92 checks 0 FAIL.

Advisor review fix (folded into the commit, now 528a2a3c7): a READ_ACK of MsgID 99 while the push WRITE is pending (answer to an earlier READ 99: the gateway's slow poll or the thermostat's) no longer ends the push; PIC ID99ReadData turns such reads into writes while opermodewrite is set (gateway.asm:3253-3257). New test case red before the guard, green after; 7 OK. Enum check: vendored OpenTherm.h READ_ACK=0b100, WRITE_ACK=0b101 (UNKNOWN_DATA_ID 0b111), so (uint8_t)respType gives 4/5/7. Bench limit: on .61 in loopback the push hooks do not run (loopback answers never reach handleMasterResponse), so .61 can show HW=P -> R10631000 and PR=W unchanged, not the boiler-driven end or the spoof path; AC4 (capture of one push) needs a real boiler or the slave simulator.

Full scripts/run_tests.py on the integrated HEAD (6841ab5d9, -j 4): 210 files, 208 passed, 2 failed. (1) test/host/test_override_reply.py: its PowerShell slicer anchored on 'enum DHWPushState' (gone with this task) and lacked otCH2Enable (TASK-1833) and the push hooks; fixed in a follow-up test commit (anchor matches the old enum or the new otPushManual flag, so older revisions still slice; otCH2Enable stub; no-op push hooks, the push is measured by tests/test_otd_dhw_push_pic.py). Rerun: 4 OK. I missed it before because I ran the tests/ suites, not test/host/. (2) test/host/test_stream_real_cbuf.py failed only under -j 4; alone it passes (exit 0, 'PASS: green with the presize, red without it'); not related to these changes, reported to the coordinator.
<!-- SECTION:NOTES:END -->
