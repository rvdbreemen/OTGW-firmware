---
id: TASK-1401
title: >-
  SAT PWM: count only PWM's own ON phases against cycles per hour (entry 59, Q12
  A)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T20:29:00Z'
updated_date: '2026-10-29T19:16:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 540000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 19:52 (Q12: A, entry 59; context entry 58). thermo-nova counts only its own PWM ON-phase starts (pwm.py:159-160) in a rolling hour (:138-141); hot-water and relight flame starts never count. Firmware counts every flame-on edge in the rolling hour and blocks a new PWM ON phase at cycles per hour (SATcycles.ino:279-299), which caused gruijter's 56-minute heating gap after a shower (Q12 evidence). Change: the hourly counter counts PWM ON-phase starts only; limit = cycles per hour. Note thermo-nova's own check compares with duty_cycle_seconds (pwm.py:156) and so never blocks; entry 59 asks for the cycles-per-hour limit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Hot-water and relight flame starts do not count; only PWM ON-phase starts count against cycles per hour (host test red on HEAD with the shower scenario)
- [x] #2 A 4th PWM ON start in the rolling hour at 3 cycles/h is still blocked
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-26 Robert: implement now with priority, together with TASK-1369 (PWM-entry ON counts, never blocked by the limit; see TASK-1369 notes).

2026-10-26 implemented (workflow wf_ab1ccec1: implement, two refuting reviews, fix; main-session re-run): tests/test_sat_pwm_start_limit.py 17 OK on the fix, FAILED (failures=15) against HEAD sources; all tests/test_sat_*.py and test_otd_ch_override OK; evaluate --quick 0 failed. The hourly counter has one writer (satCycleRecordPwmStart); hot-water and relight flame edges no longer count; cycles_this_hour now shows PWM ON phases. Deviations from thermo-nova (provisional, queued as Q49-Q51): count at the first flame of the phase (not at the ON start), ON timing from the phase start when the flame already burns, off wait from the ON-to-OFF edge while the flame is lit. Not built/benched yet.

2026-10-27 bench, Classic + S3 Mini with live PIC (.63, PIC 6.8 pic16f1847, no boiler), same replay on both builds (1552 lines: 3 hot-water flame starts of 18 s, then a 600 s heating burn with flow 45-48 vs setpoint 28.5 + margin 3.0), settings restored (GET diff empty), logs in %TEMP%\otgw-bench-514\. OLD alpha.513: cycles_this_hour counted the three hot-water starts (0,1,2,3) and 4 after the heating ignition; switch to PWM at 00:03:23.861 (sustained overshoot 60 s), then CS=10.0 queued at 00:03:53.873 (30 s later) and repeated every tick: the burn is cut, as in gruijter's 12:31 cut. NEW alpha.514+5b2afb5 (OTA: 'Update OK', running 514, settings changed: none): cycles_this_hour stayed 0 through the three hot-water starts and the heating ignition; switch at 00:22:21.995, CS 43.3 -> 44.4 ... 45.2 (about flow minus 1, entry 22) for 4 minutes, pwm_flame_req true, cycles_this_hour 1 (the entry ON phase counted); first CS=10.0 at 00:26:22.149 = the regular ON to OFF end, no cycle-limit line. Limits: the old build's 'cycle limit 3/hr reached' line did not appear on the bench (the cut there is probably the OFF wait after entry, TASK-1369 audit 17; not traced), replay runs about 1.5x slower than designed, a 4th OFF->ON block was not exercised on the bench (host test (b) covers it).

TASK-1412.1 records review (R31): 2026-10-29. (TASK-1412.1, R31 record reconciliation; history above unchanged): (1) The description's 'thermo-nova counts ... in a rolling hour (:138-141)' is imprecise. pwm.py:138-141 is an anchored hour that restarts, and reset() clears it (pwm.py:90). Only the firmware ring slides (SATcycles.ino:328-344). (2) 'Never blocks' holds at the default duty_cycle 780 (const.py:167, entry_data.py:220); pwm.py:155 limits starts to fewer than about 20 per hour. (3) Of the provisional deviations in the Final Summary, 'count at the first flame' was reversed by Q49 B (entry 90, TASK-1421). The other two were confirmed by Q50 A / Q51 A (entry 91) and Q51b (entry 97). (4) Evidence class: host tests plus a no-boiler PIC replay bench. AC#2 was host-only. This task does not certify physical start counts or comfort (LL-023). See the correction entry added under R31.

TASK-1412.1 records review (R37): 2026-10-29. (TASK-1412.1, R37): the provisional deviations put to Sergeant D as Q49-Q51 are decided. Q49 B, count at the ON start as pwm.py:160 (entry 90): implemented in TASK-1421 (932cda228, alpha.517), after the Q35 fix TASK-1382 (de23e3ea1, alpha.515; ancestry checked with git merge-base --is-ancestor), which is the order entry 90 requires. Q50 A and Q51 A (entry 91) confirm this task's anchors. Q51b, follow thermo-nova (entry 97): OFF always timed from the ON->OFF switch, implemented in TASK-1380 (de23e3ea1). The first-flame count described above no longer exists (SATcontrol.ino:876-883, :1191).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The hourly start limit (cycles per hour) now counts only SAT's own PWM ON phases (entries 58, 59, Q12 A); hot-water and relight flame starts no longer count, so a shower no longer blocks heating for ~54 minutes (gruijter 2026-10-26). Commit 5b2afb580 (alpha.514). AC#1: tests/test_sat_pwm_start_limit.py (17 cases, real code): OK on the fix, 15 failures on the HEAD sources; bench old vs new above: 513 counts the hot-water starts (3 before the heating burn), 514 counts 0. AC#2: host scenario (b) 3 PWM starts in the hour -> the 4th OFF->ON is blocked on both HEAD and the fix; not exercised on the bench. All tests/test_sat_*.py OK, evaluate 0 failed, build.bat SUCCESS. Provisional deviations from thermo-nova, put to Sergeant D as Q49-Q51: count at the first flame of the phase (not at the ON start, pwm.py:160), ON timing from the phase start when the flame already burns (pwm.py:162-167), off wait from the ON-to-OFF edge (pwm.py:208); the PWM-entry ON phase counts but is never cut by the limit (Robert 2026-10-26, Q48). cycles_this_hour now shows PWM ON phases.
<!-- SECTION:FINAL_SUMMARY:END -->
