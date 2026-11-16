---
id: TASK-1971
title: PIC settings cycle waits 4-5 min after boot while the MQTT discovery drip runs
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T11:48:00Z'
updated_date: '2026-11-16T07:38:00Z'
labels:
  - pic
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1008000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .63 (beta.306+041400e, 2026-11-15): right after a reboot the PIC panel stays empty (0/15 after 80 s). queryNextPICsetting() skips while dripDueWithinMs(500) (OTGW-Core.ino ~1300) and the MQTT discovery drip ticks every 1 s, so the PR= cycle barely gets a slot until the drip is done, about 4-5 min after boot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a boot the PIC settings fill within about 60 s also while the discovery drip runs
- [x] #2 The ADR-088 status-burst window and cooldown stay intact (evaluate gates green); host test or trace on the real scheduling shows the cycle advancing during the drip, red on the old code
- [x] #3 Bench: reboot .63, /api/v2/pic/settings 15/15 within 60 s
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Cause (code reading, 2026-11-16): doTaskEvery3s (timer3s, 3 s, SKIP_MISSED_TICKS) calls queryNextPICsetting(), which returns when dripDueWithinMs(500); the drip ticks every 1 s from its own timer. Both run at a fixed cadence from boot, so the 3 s tick keeps the same phase against the drip: when it lands within 500 ms before a drip tick, it is deferred on every tick until the drip has nothing pending (4-5 min). Fix: the loop calls a new pollPICsettingsQuery() right after loopMQTTDiscovery(); while a cycle is active it runs queryNextPICsetting() once at least 3 s passed since the last sent query and the gates (burst, drip) are clear; queryNextPICsetting() returns whether it sent. A deferred query then goes out right after the next drip tick (< 1 s later) and the 3 s spacing stays. doTaskEvery3s keeps only liveness + pollPicTempSensor. ADR-088: the burst and drip gates stay, unchanged. Test: tests/test_pic_settings_cycle_drip.py compiles the real dripDueWithinMs, triggerPICsettingsReadout/queryNextPICsetting (+ pollPICsettingsQuery when present) and doTaskEvery3s with a simulated loop (3 s task, drip, poll), drip pending, worst phase (drip 300 ms after the 3 s tick): old 0 PR= in 60 s (red), fix 15 within 60 s and >= 3 s apart; plus phase 700 ms and no drip. Bench AC3 via 09 on .63 after the release. Not on 1.x: different scheduler (check later).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Repro + fix (session 95, 2026-11-16 06:05-06:18): tests/test_pic_settings_cycle_drip.py compiles the real dripDueWithinMs (MQTTstuff.ino), the real PIC readout block (OTGW-Core.ino, picSettingsCycleActive..handlePRresponse) and the real doTaskEvery3s, loop simulated in firmware order (3 s task, drip tick + due time, pollPICsettingsQuery when present), 10 ms steps. Old code (origin/dev e255ab20b): worst phase (drip 300 ms after the 3 s tick) 0 PR= in 60 s -> FAILED (failures=2: 0 != 15, no gaps); phase 700 ms and no-drip complete (the phase lock). Fix: Ran 4 OK (15 within 60 s, >= 3 s apart, none within 500 ms before a drip tick, other phases OK). tests/test_pic_temp_sensor_port.py adjusted (no early return before pollPicTempSensor) 8 OK. Sweep of 25 tests touching doTaskEvery3s/queryNextPICsetting/drip/statusBurst/OTGW-firmware.ino: all OK. evaluate --quick 88 pass 0 fail (ADR-088 gates included). Opus review running.

2026-11-16 bench 09 on .63 (Classic, PIC 6.8), old-vs-fix on the same reproduction: an esptool otadata dump over COM8 (USB reset, lastreset 'USB reset (host)'), then GET /api/v2/pic/settings every 5 s, counting non-empty fields of 15. OLD 2.0.0-beta.339+bd7a133 (08:03:54): REST up 9.4 s, 0/15 at 80 s, first field 228.5 s, 15/15 at 274.9 s. FIX 2.0.0-beta.340+7c12dfd (sha ino de3eb274..006b confirmed by 95; 08:14:30): REST up 9.3 s, fields from the first answer (9.3 s), 15/15 at 54.9 s. MQTT connected in both; bootcount 151 -> 153 (OTA) -> 154 (dump); no crash fields; settings.ini and /api/v2/settings byte-identical to before the flash. SAT is enabled and active on .63 (its standing config), so the drip and SAT ran as in the field. Evidence: 09 scratchpad t1971/ (poll-old339.log, poll-fix340.log, picpoll.py, flash340.log).

Pushed 56e4250ec..8116b7195 (rebased onto 56e4250ec, src diff vs the benched 7c12dfd5b = 0). Rebuild from 8116b7195: exit 0, 'Build completed successfully', app size 2139408 B of 2293760 B (93.3 %), free 154352 B. Zip OTGW-firmware-esp32-combo-2.0.0-beta.340+8116b71-flash.zip posted in #alpha-testing (message 1571788979836293150). Status In Review: 09 ticks AC3.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: 09 bench .63 (Classic, PIC 6.8), same USB-reset reproduction on both builds: OLD beta.339+bd7a133 0/15 PIC fields at 80 s, first field at 228.5 s, 15/15 at 274.9 s; FIX beta.340+7c12dfd fields from the first REST answer (9.3 s), 15/15 at 54.9 s; MQTT connected, no crash fields, settings byte-identical (09 scratchpad t1971/). AC2: evaluate.py --quick 88/0 (incl. the ADR-088 burst/cooldown/drip gates); tests/test_pic_settings_cycle_drip.py runs the real dripDueWithinMs, readout block and doTaskEvery3s: fix 5/5, old code 3/5 red (0 PR= in 60 s at phase 0 and 300 ms). AC3 (bench): the 09 run above, ticked by 09. Not verified: the 4-5 min drift timing itself (the simulation fixes the phase).
<!-- SECTION:FINAL_SUMMARY:END -->
