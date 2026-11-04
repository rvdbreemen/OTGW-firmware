---
id: TASK-1369
title: 'SAT PWM: the first phase after PWM starts is ON (entry 43)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T21:43:00Z'
updated_date: '2026-10-29T19:16:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 504000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q11, entry 43): first PWM phase is ON, as thermo-nova pwm.py (audit #17: firmware starts OFF timed from the last flame-off).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 When PWM starts the first phase is ON at once (host test red/green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-26 Robert (maintainer decision, provisional until Sergeant D answers Q48): the ON phase that starts when PWM is entered with the burner already running (entries 22, 43) COUNTS as a PWM ON start for the hourly limit (entry 59, TASK-1401) but the limit never blocks or cuts that entry start. Rationale: entry 43 literally; thermo-nova counts it (pwm.py:160) and its cap check (pwm.py:156) compares with duty_cycle_seconds, so in practice it never blocks. Removes gruijter's 12:31 cut (2026-10-26 capture).

2026-10-26 implemented together with TASK-1401 (same PWM start gate): the PWM-entry ON phase (burner already burning) starts at once when the on time is >= 180 s (pwm.py:154-155, const.py:18), counts as a PWM start, and is never cut by the hourly limit (maintainer decision, provisional Q48). Tests in tests/test_sat_pwm_start_limit.py (entry, entry5, entryflow, entrylimit, lag, wrap cases).

2026-10-27 bench, Classic + S3 Mini with live PIC (.63, PIC 6.8 pic16f1847, no boiler), same replay on both builds (1552 lines: 3 hot-water flame starts of 18 s, then a 600 s heating burn with flow 45-48 vs setpoint 28.5 + margin 3.0), settings restored (GET diff empty), logs in %TEMP%\otgw-bench-514\. OLD alpha.513: cycles_this_hour counted the three hot-water starts (0,1,2,3) and 4 after the heating ignition; switch to PWM at 00:03:23.861 (sustained overshoot 60 s), then CS=10.0 queued at 00:03:53.873 (30 s later) and repeated every tick: the burn is cut, as in gruijter's 12:31 cut. NEW alpha.514+5b2afb5 (OTA: 'Update OK', running 514, settings changed: none): cycles_this_hour stayed 0 through the three hot-water starts and the heating ignition; switch at 00:22:21.995, CS 43.3 -> 44.4 ... 45.2 (about flow minus 1, entry 22) for 4 minutes, pwm_flame_req true, cycles_this_hour 1 (the entry ON phase counted); first CS=10.0 at 00:26:22.149 = the regular ON to OFF end, no cycle-limit line. Limits: the old build's 'cycle limit 3/hr reached' line did not appear on the bench (the cut there is probably the OFF wait after entry, TASK-1369 audit 17; not traced), replay runs about 1.5x slower than designed, a 4th OFF->ON block was not exercised on the bench (host test (b) covers it).

TASK-1412.1 records review (R37): 2026-10-29. (TASK-1412.1, R37): the 2026-10-26 maintainer decision marked 'provisional until Sergeant D answers Q48' is no longer provisional. Sergeant D answered Q48 A on 2026-10-28 05:07 UTC (sergeant-d-corrections.md entry 89; the entry keeps the maintainer provenance). He added that a hot-water flame never counts (entries 89, 95 Q48b; refined by 101/105/106). Implemented in TASK-1421 (932cda228, alpha.517) and TASK-1422. Current code: entry-ON limit exemption at SATcontrol.ino:1171, counted at the ON start at :883/:1191. The history above stays as written. This closes the record only; it does not certify physical performance.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
When PWM is entered with the burner already burning, the first PWM phase is ON at once (entry 43, entry 22: CS = flow - 1, no cut), as thermo-nova starts ON from IDLE (pwm.py:154-177) when the on time is at least 180 s; the entry ON phase counts as a PWM start and is never cut by the hourly limit (maintainer decision 2026-10-26, provisional, Q48). Commit 5b2afb580 (alpha.514), done together with TASK-1401. Evidence: tests/test_sat_pwm_start_limit.py entry cases red on the HEAD sources, green on the fix (17 OK); bench old vs new on the real device: alpha.513 cut the burn 30 s after the switch to PWM (CS=10.0 from 00:03:53), alpha.514 kept CS about flow minus 1 for 4 minutes and ended the phase normally (CS=10.0 at 00:26:22). Not verified on the bench: the old build's limit log line and the exact cause of the old cut.
<!-- SECTION:FINAL_SUMMARY:END -->
