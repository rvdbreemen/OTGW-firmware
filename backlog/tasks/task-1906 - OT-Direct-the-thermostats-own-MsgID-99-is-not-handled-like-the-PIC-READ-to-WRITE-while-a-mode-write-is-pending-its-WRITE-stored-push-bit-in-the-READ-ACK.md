---
id: TASK-1906
title: >-
  OT-Direct: the thermostat's own MsgID 99 is not handled like the PIC (READ to
  WRITE while a mode write is pending, its WRITE stored, push bit in the
  READ-ACK)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T18:48:00Z'
updated_date: '2026-11-16T08:14:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 972000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Orphan from the 2026-11-12 PIC 6.8 parity audit (REVIEW.md N11 'thermostat's own MsgID 99 not ...', explicitly left out of TASK-1845 with no AC); found again by the TASK-1851 delta check. PIC 6.8: a thermostat READ 99 is turned into a WRITE carrying the gateway's operating mode while opermodewrite is set (ID99ReadData, gateway.asm:3253-3257); a thermostat WRITE 99 is stored as the operating mode (ID99WriteData, :3267-3271); a boiler READ-ACK 99 gets the push bit set while the gateway spoofs a push (ID99ReadAck, :3259-3265). OT-Direct: by grep, MsgID 99 code covers only the gateway's own writes (OTDirect.ino:2677, :3616-3646) and the boiler reply hook (:1865); no code in the relay path rewrites or stores a thermostat MsgID 99 (grep-based, the relay path not traced line by line).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: with an MW=/MH=/M2= or HW=P write pending, a relayed thermostat READ 99 goes to the boiler as WRITE 99 with the gateway's mode; red on the current code
- [x] #2 Host test: a thermostat WRITE 99 updates the stored operating mode used by later MW/MH/M2 writes
- [x] #3 Host test: during a spoofed push the READ-ACK 99 answered to the thermostat has the push bit set
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
95 (2026-11-15, lane-95-next): PIC 6.8 MessageID99 :3219-3279 read in full (ID99ReadData :3252-3256, OperModeAck/OperModeData :3245-3250, ID99WriteData :3268-3279 with StartDHWPush, ID99ReadAck :3258-3265, OperModeBoiler :3239, PushOperModes :3281-3288). New flag otOperModeWrite (= opermodewrite): set by MW=/MH=/M2= and otdPushStart(), cleared by a boiler WRITE_ACK 99 (otdPushOnBoiler99) and resetTransientState(). New '// --- otdOper99' block: otdRelayRead99() (applyOverrides, gateway mode: thermostat READ 99 -> WRITE 99 with otdOperModeData), buildOverriddenReply() answers that read with the gateway's modes; otdOnThermostatWrite99() (loopOTDirect gateway branch next to onThermostatMsgID16) stores DHW/HC1/HC2 and a push bit starts a push (pending, or spoof when 99 is known unsupported; no extra write, the thermostat's own write carries the bit); otdReply99() in replyToThermostat sets the push bit on a boiler READ_ACK 99 while the gateway spoofs. Not ported: PushOperModes also sets dhwpushpending for a plain MW=/MH= write, so the PIC would spoof a push on that WRITE_ACK; OT-Direct keeps the TASK-1845 rule (pending only for HW=P). Test tests/test_otd_msgid99_relay.py (real applyOverrides, buildOverriddenReply, MW=/MH= branches, otdPush + otdOper99 blocks): 5/5; on origin/dev c2cd68b16 the 3 AC tests FAIL (read passes unchanged, thermostat write ignored, no push bit). 12 harnesses got stubs (they cut applyOverrides/buildOverriddenReply/loop or the push block); forward declarations without parameter names so test_otd_dhw_push_pic's signature lookup finds the real otdOperModeData. Sweep 52 test_otd_*/sat/pm_chain/hoisting/evaluate + 8 other OTDirect slicers + 3 ps1 harnesses green; evaluate --quick 86/0. Not verified: a real thermostat that reads/writes MsgID 99 (bench needed).

Bench 09 on .61 (OT-Direct, no boiler, no thermostat), beta.298+bb38dc5 (bench-only build of lane-95-next on c2cd68b16, 2026-11-15 07:25, exit 0, RAM 130572 B, Flash 2156679 B): regression PASS (MW=/MH=/M2= replies and WRITE 99 as before, HW=P as TASK-1845). NOT benched (field only): AC1-AC3 need a thermostat that reads/writes MsgID 99; host test tests/test_otd_msgid99_relay.py.

Landed: Pushed origin/dev 2e4ca9c05..3e29faa63; zip beta.301+2b3d9a2 posted (Discord 1571471374038470711); final build exit 0 'Build completed successfully', RAM 130572 B, Flash 2157483 B; sweep 88/88, evaluate --quick 86/0; bench 09 on .61 beta.301+2b3d9a2 PASS (smoke + re-check). Status In Review (convention: field ACs open, see AC list; host-proven).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix afc495f5a (PIC 6.8 MessageID99 :3219-3279 read in full: ID99ReadData, OperModeAck/OperModeData, ID99WriteData with StartDHWPush, ID99ReadAck). AC1-AC3: tests/test_otd_msgid99_relay.py 5/5 on the fix; the 3 AC tests FAIL on c2cd68b16 (READ 99 relayed as WRITE 99 with the gateway mode while an MW/MH/M2/HW=P write is pending; a thermostat WRITE 99 updates the stored mode; the spoofed-push READ-ACK 99 carries the push bit). Landed: origin/dev 2e4ca9c05..3e29faa63, zip beta.301+2b3d9a2 (Discord 1571471374038470711), build exit 0, sweep 88/88, evaluate --quick 86/0; bench 09 on .61 (beta.298 bench build and beta.301) regression PASS for MW=/MH=/M2= and WRITE 99. Not benched: a real thermostat sending its own MsgID 99 (none on the bench); a field check is a follow-up, not an AC.
<!-- SECTION:FINAL_SUMMARY:END -->
