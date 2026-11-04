---
id: TASK-1570
title: >-
  SAT: send MM, TP=11:12 and CH only on change, for every boiler; CS keeps the 5
  s resend (Sergeant D entry 237)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T20:56:00Z'
updated_date: '2026-11-04T10:42:00Z'
labels:
  - sat
dependencies: []
priority: high
ordinal: 717000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 237 (2026-11-02, answers to Q-1537c): MM= (and for an explicit Immergas its TP=11:12=min(MM,80)) and CH= go to the PIC only when the value changes, for every boiler; CS= keeps the 5 s resend and the PIC keepalive. Changes entry 44 / ADR-203 for MM and CH. Firmware today: satSendMM()/satSendCH() enqueue on every control tick (SATcontrol.ino ~6862), on the 5 s satCmdResend() (:694-702) and on the hot-water/cutoff paths (:6615/6645/6661). PIC keeps MM= and CH= until a new command (gateway.asm:4752-4764; only SC/CS/C2 expire, :493-496, :1619-1641), so on-change is safe as long as a lost value is resent: SAT taking control, OT link back, PIC reset, reboot (satResetCmdCache).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real satSendMM/satSendCH/satCmdResend: an unchanged MM or CH is not enqueued again by the tick or the 5 s resend; a changed value is enqueued once; Immergas TP=11:12 follows MM; CS is still resent every 5 s (red on beta.17, green)
- [x] #2 After a command-cache reset (SAT takes control, OT link back, PIC reset detected, reboot) the next send enqueues MM, CH (and the Immergas TP) once even if unchanged; the PIC reset path is identified in the code and covered by the test
- [x] #3 A Proposed ADR superseding ADR-203 for MM and CH (CS unchanged), citing entry 237; the maintainer accepts it
- [x] #4 Bench .63 telnet: MM/CH/TP enqueue counts per minute before (beta.17) and after; CS still every 5 s; settings kept, stable 5 min
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) satSendMM/satSendCH: skip an unchanged value while _sat_mmSentValid/_sat_chSentValid is set and the last send is < SAT_CMD_REFRESH_MS (300 s) old; satResetCmdCache clears the flags. 2) satCmdResend repeats CS only. 3) satNotifyPicRestart() (= satResetCmdCache) from the runtime PIC banner branch in processOT (loop task, OTGW-Core.ino:560-575 drain; PR: A goes to handlePRresponse first, :5767). 4) Direct MM=/CH= writes outside the helpers (satDisable :3507/3512, restart hand-back :3647/3648) are each followed by satResetCmdCache: checked. 5) 300 s repair = our addition (advisor review: addCommandToQueue returns void so a dropped enqueue was otherwise never repaired; external MM=/CH= now stays up to 300 s), put to Sergeant D as Q-1570a. 6) ADR partly superseding ADR-203 (Proposed, Robert accepts). 7) Tests: test_sat_cmd_cache red on beta.17 then green; harness updates in immergas_tp/ot_loss/push_setpoint. 8) Release: beta.17 stays the 08:00 candidate; 1570 flashed after 08:00 (alpha track). Bench AC2 runtime PIC restart needs Robert's explicit order (GW=R / resetgateway).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02: tests/test_sat_cmd_cache.py 18/18 on the fix, beta.17 copy 9 failures + 1 error; test_sat_immergas_tp 5/5, ot_loss 24/24, push_setpoint 10/10; evaluate --quick 0 failed. Full tests folder running.

Robert 2026-11-02 evening: (1) 'akkoord' on the ADR partly superseding ADR-203 (accept once the adr-kit gates pass); (2) 'ja, je mag een reset geven op de bench': explicit order for a PIC reset (GW=R / resetgateway) on bench unit .63 for AC2, after the 08:00 public beta. Gateway reset only, no PIC flash.

Full tests/ 97 files: 96 green, test_sat_cs_ceiling_1536 failed on 2 assertions that expected the 5 s resend to repeat MM=100;CH=1 (replaced by entry 237; the CS ceiling values stay checked); expectations updated, now green. ADR-207 Proposed (adr-kit, governance 15/15, quality 0.92); docs + CHANGELOG updated by agent. Entry 238 recorded (Sergeant D confirms the resend after a reset). Waiting: Q-1570a (5-min repair), asked again msg 1566924494524911667.

2026-11-02: AC1 tests/test_sat_cmd_cache.py 18/18 (beta.17: 9 failures + 1 error). AC3 ADR-207 Accepted (maintainer akkoord, adr-kit gates passed), related to ADR-203 both sides. AC2 code path identified and covered by the host test (banner branch -> satNotifyPicRestart, structural test); bench part (GW=R on .63) pending after 08:00. AC4 bench pending. Repair window 60 s per entry 239.

2026-11-04 bench .63 beta.68 (simulation on, 5.5 min telnet): CS enqueued every 5.0 s (25 in 125 s), SATTICK every 30 s, MM and CH on change and as repair every 90.0 s (expected 60 s per entry 239: defect filed as TASK-1602). With simulation off and no boiler SAT hands back after 30 s without a boiler reply (CS=0, CH=1, MM=T), as designed.

2026-11-04 AC2: reset paths in the code: satResetCmdCache() (SATcontrol.ino:604) on hand-back (:3797), SAT (re)enable (:3817), OT link back (:3895, SAT_OTLINK_RESUME), outside-temp missing (:3982); PIC restart: the PIC banner in OTGW-Core.ino:6053 calls satNotifyPicRestart() (SATcontrol.ino:741); reboot: the flags are static false. tests/test_sat_cmd_cache.py 28 OK incl. CmdCacheReset (4 reset timings), test_mm_and_ch_only_on_change_and_again_after_pic_restart, test_pic_banner_calls_the_restart_hook. Bench .63 beta.69: after the OTA reboot MM and CH went out once, then on the 60 s repair cadence.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT sends MM, the Immergas TP and CH only on change (entry 237) with a 60 s repair (entry 239; the 90 s slip fixed in TASK-1602); CS keeps the 5 s resend. A cache reset (hand-back, SAT enable, OT link back, PIC banner, reboot) sends them once again. Evidence: tests/test_sat_cmd_cache.py 28 OK, bench .63: CS every 5.0 s, MM/CH every 60.0 s (beta.69).
<!-- SECTION:FINAL_SUMMARY:END -->
