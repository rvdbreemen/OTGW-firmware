---
id: TASK-1236
title: >-
  SAT on PIC boards: control setpoint lapses between 300 s refreshes (PIC CS
  expiry ~60 s)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 15:35'
updated_date: '2026-10-16 03:54'
labels:
  - sat
  - pic
  - bug
dependencies: []
priority: high
ordinal: 374000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the ADR-179 acceptance check (2026-10-15). other-projects/otgw-6.6/gateway.asm:1619-1630 (CommandExpiry): when no CS command arrived in the past minute (HeartbeatCS clear) and controlsetpt1 & 0xF8 != 0 (setpoint >= 8 C), the PIC sets SysCtrlSetpoint and drops the override; the thermostat drives TSet again. SAT re-sends unchanged commands only every SAT_CMD_REFRESH_MS = 300000 ms (SATcontrol.ino:317). On PIC boards (esp32-classic, combo in PIC mode) SAT's steady setpoint therefore likely lapses about 60-120 s after each send. The OTDirect path skips its own CS expiry while SAT owns MsgID 1 (OTDirect.ino:2148), so this is PIC-only. Single-source reading of the assembly, not yet reproduced on hardware. Also: external CS= is not gated by satOwnsControlSetpoint() on PIC boards (addCommandToQueue reaches handleOTDirectCommand only without PIC, OTGW-Core.ino:3616-3623).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench reproduction on the Classic-S3 (PIC): capture shows TSet returning to the thermostat value within ~2 min after a SAT CS= while SAT holds a steady setpoint (old code)
- [x] #2 Fix: SAT re-sends its CS= on PIC boards within the PIC expiry window (<= 50 s), without raising the OTDirect cadence
- [x] #3 Same capture on the fix shows TSet held at the SAT value for >= 10 min
- [x] #4 Decide and record whether an external CS= is refused while SAT owns MsgID 1 on PIC boards (ADR-179 scope)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Bench first (AC#1): run scripts/pic_cs_expiry_probe.py on the Classic-S3 on the current build: (a) SAT off, --send-cs 45 --duration 240; (b) SAT on, --duration 900. Old-code evidence.
2. Fix (AC#2), smallest change: in satEnqueueIfChangedCS() (SATcontrol.ino:348) use a CS-only refresh window of 30 s when the board runs in PIC mode (runtime check, so esp32-combo picks it per boot), keep SAT_CMD_REFRESH_MS = 300 s for MM/CH/TC and for OT-Direct. Only CS needs it: gateway.asm expires SC, CS and C2 only. Cost: one 'CS=x.x' line on the PIC serial link per 30 s. Check that the SAT loop calls the CS path at least every 30 s; if not, the window does not help and the plan needs a timer instead.
3. Same probe on the fix (AC#3): override held >= 10 min.
4. AC#4 is a policy question for the maintainer: refuse external CS= on PIC boards while SAT owns MsgID 1 (parity with OT-Direct, ADR-179) or leave it open. Recommendation: refuse, same rule on both board types.
Waiting for the Classic-S3 and the maintainer's go on step 2 and the AC#4 choice.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-15: expiry window computed from gateway.asm. Timer1 = Fosc/4 (4 MHz internal osc, OSCCON b'01101010') with 1:8 prescale, 16-bit overflow = 0.524 s; StartTimer loads minutetimer=120 and clears the heartbeat flags; Expired = bit 7 after 121 ticks = ~63 s. A CS= starts the timer; at the first expiry the heartbeat is set, so the timer restarts with cleared flags; at the second expiry (~126 s) without a new CS= the PIC sets SysCtrlSetpoint. So SAT's 300 s cadence leaves the override off for roughly 174-237 s of every 300 s. Refresh must be < 63 s to be safe in all phases (AC#2 says <= 50 s). Added scripts/pic_cs_expiry_probe.py: reads the PIC stream on port 25238, follows MsgID 1 T/R frames and 'CS: x' acks, reports override ON/OFF transitions with time since the last ack. Smoke-tested on synthetic lines; bench run waits for the Classic-S3 (not connected today). Bench recipe: (1) --send-cs 45 --duration 240 with SAT off: expect override OFF ~63-126 s after the ack; (2) SAT on, --duration 900: expect OFF phases between SAT's 300 s acks.

2026-10-15: maintainer approved the plan and chose to REFUSE external CS= on PIC boards while SAT owns MsgID 1 (AC#4). Implemented in alpha.427: (1) satPicCSKeepalive() in SATcontrol.ino re-sends the last SAT CS every 30 s on PIC boards (isPICEnabled, last CS >= 8 C), called from satLoop every pass before the control-timer gate, so it does not depend on the control interval (10-300 s, default 30). OT-Direct untouched. (2) addCommandToQueue() (OTGW-Core.ino, PIC path only) refuses CS= while satOwnsControlSetpoint() unless satCommandInFlight(), same rule as the OT-Direct handler. Raw bytes on port 25238 bypass the queue and stay unfiltered (ADR-177). All internal CS= producers already use satEnqueueOwnCS (checked by grep). AC#1/#3 still need the Classic-S3 bench run with scripts/pic_cs_expiry_probe.py on alpha.426 (old) and alpha.427 (fix).

Evidence alpha.427 (8815ddb0d): build.bat --firmware --target all SUCCESS for esp32 (20:32), esp32-classic (20:38), esp32-combo (20:43; flash 88.2%); evaluate.py --quick 71 pass / 0 warn / 0 fail. OTGW32 bench (OT-Direct, .88.61) flashed with the combo app: boots alpha.427, otdirectavailable=true, /api/v2/sat/status answers with SAT enabled (keepalive is a no-op without PIC). Remaining: AC#1 and AC#3 on the Classic-S3.

Bench 2026-10-15/16, Classic-S3 (COM8, .88.63, PIC 6.8 gateway, no boiler/thermostat: the stand-alone PIC polls MsgID 0, whose HB bit 0 (CH enable) is set while a user CS is active and CH mode is on). Logs in .tmp/bench-1236/. OLD: (a) alpha.424, CS=45 once: CH bit set at 2.7 s, dropped at 67.0 s, 65 s after the ack; (b) alpha.424 SAT on: one 'CS: 10.00' in 400 s (300 s cadence); (c) alpha.426 (pre-fix, built from 543dc4e) harness T2 FAIL 1 ack in 330 s, longest gap 261 s; T3 FAIL external CS=45 acked while SAT on. FIX alpha.427: harness T1a PASS CH bit held over 170 status frames with CS every 30 s; T1b PASS dropped 102.6 s after the last ack (window 63-126 s); T2 PASS 22 acks in 660 s, longest gap 33.0 s; T3 PASS external CS=45 not acked; T4a PASS SAT release CS=0 acked; T4b PASS no CS acks in 70 s after disable; T5 PASS external CS=45 accepted with SAT off. AC#3 method note: with no boiler TSet itself is not visible; the proof is T1 (a CS refreshed within 30 s holds the PIC override) plus T2 (SAT refreshes within 33 s for 11 min). Harness gotchas found and fixed in the script: SAT's CH=0 persists in the PIC and masks the CH bit (send CH=1 first); the status bits are in the high byte. Bench restored: alpha.427, SAT off, port 25238 off.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT on PIC boards now keeps its control setpoint: satPicCSKeepalive() re-sends the last CS every 30 s (the PIC drops a CS 63-126 s after the last one, gateway.asm CommandExpiry), and addCommandToQueue() refuses an external CS= on the PIC path while SAT owns MsgID 1, matching the OT-Direct rule (ADR-179, maintainer decision). Shipped in alpha.427 (8815ddb0d). Evidence per AC: AC#1 old-code bench runs on alpha.424/426 (override lapses 65 s after a CS, SAT acks 300 s apart, T2/T3 FAIL); AC#2 build.bat all three targets SUCCESS, evaluate --quick 0 fail; AC#3 harness T1+T2 PASS on alpha.427 (11 min, max gap 33 s); AC#4 T3/T4/T5 PASS (external refused, own release passes, external accepted again with SAT off). Tools: scripts/pic_cs_expiry_probe.py, scripts/verify_pic_cs_keepalive.py. Limitation: bytes written straight to port 25238 bypass the queue and are not filtered (ADR-177).
<!-- SECTION:FINAL_SUMMARY:END -->
