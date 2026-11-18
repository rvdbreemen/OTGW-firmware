---
id: TASK-1989
title: >-
  Periodic loop stalls of about 600 ms every 10-20 min (gruijter beta.250): find
  the cause on a current build
status: To Do
assignee: []
created_date: '2026-11-15T21:23:00Z'
updated_date: '2026-11-18T11:51:00Z'
labels:
  - performance
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1017000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter beta.250 (Classic with PIC, SAT on), 25.5 h: 92 '[loop-stall]' lines, median 420 ms, max 607 ms (15:15:30), about 3-4 per hour, often at :x5:30/:x0:30; just before the 607 ms one a CS=10.0 send and a MsgID 2 Write-Ack. Far below the 30 s task watchdog, no visible effect. Not covered by the earlier weather-fetch stall fixes (TASK-1478, TASK-1604).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce on a current build (bench Classic with SAT, or the replay) and name the code that runs during a 400-600 ms stall, with evidence (telnet timing or a loop profiler)
- [ ] #2 Fix it or record why it stays (bounded, harmless), with old-vs-fix numbers when fixed
- [ ] #3 The queued writes still land: on .63 (SAT active, PIC + room sensor) /sat_pid_state.json is rewritten every 5 min, its integral 'i' matches pid_i of GET /api/v2/sat/status after a /ReBoot (drain before prepareForReboot), and the sat_*.json files survive a filesystem OTA byte-identical (drain before _keepFiles)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 AC1 evidence (09, .61, bench build 2.0.0-beta.342+cd59f4e, 08:07-08:18): '[loop-stall] N ms gap, slowest step 1->2 M ms' (LOOP_TOP -> TIMERS) after exactly every 6th LittleFS write of 40 on the loop task (247-338 ms), 0 stalls for the same 40 writes from a core-0 task; the code is the LittleFS write itself in the timer step (satSavePidState every 5 min, satSaveEnergyState hourly). AC2 'old' numbers are that run; the 'fix' numbers come from the post build of lane/ba-1989c on .63 after the 20:00 beta tag (boiler-less .61 runs no SAT tick, so no state writes there; coordinator 2026-11-16). Bench build 2.0.0-beta.351+597c163 (local, not posted): first build failed on fsWriterSubmit(FPSTR(...)) (const __FlashStringHelper* into const char*), fixed in 597c163ef with a faithful FPSTR stub in test_sat_pid_persist (compile error on the bug, 8/8 on the fix). 09 on .61: the sat_*.json files survive the filesystem OTA byte-identical (part of AC3).

2026-11-16 bench 09 on .61 (OTGW32, no PSRAM), 2.0.0-beta.351+597c163 (local bench build), 13:05-14:28, SAT enabled without sim: 76 min telnet, exactly one '[loop-stall]' (13:36:59.728, 591 ms, slowest step 5->8 576 ms, between logHeapStats and loopOTDirect: not the file-write step 1->2 of the 342 experiment); no step 1->2 stall, no '[fswriter] queue still full'. BUT SAT stayed active=false (ot_link_lost, no boiler, room null, 0 SATTICK), so satSavePidState/satSaveEnergyState never ran: /sat_pid_state.json kept ts 1794765369, /sat_energy.json kwh 0.000; the reboot check is trivially equal (nothing saved), not evidence. AC3 part 'files survive the fs OTA byte-identical' PASS (sat_pid_state, sat_energy, sat_markers). Health: no crash fields, heap 46.8-50.0 KB; hd_min dip to 2,180 B at the 14:00:00 wall-clock mark = the hourly sendMQTTheapdiag (26 stats/* publishes), not an fs write (the hourly energy save runs on millis() inside the SAT tick, which did not run): own task. AC2 'fix' numbers and AC3 save/reboot parts move to .63 with the post build after the 20:00 tag.

2026-11-16 hour-mark A/B by 09 on .61 (no PSRAM) for the 14:00 heap dip: 14:00 beta.351 (1989 fsWriter) + SAT enabled: hd_min 2,180 B; 15:00 beta.352 (dev, no 1989) + SAT off: no dip, 26/26 stats; 16:00 beta.353 (dev + 1751, no 1989) + SAT enabled: hd_min 31,156 -> 6,484, transient, 26/26 stats. The dip follows 'SAT enabled', not the fsWriter: 1989 is cleared of it. The dip is TASK-2004 (owner 95).

2026-11-16 tests/test_sat_loop_integration.py did not compile with the fsWriter (fsWriterSubmit not stubbed in tests/sat_loop_stubs.h; missed because the full sweep ran on the 1887n tree without 1989 and the post-branch test set left it out). Stub added (refuses, as the real fsWriterSubmit does with LittleFS not mounted); 21/21 on the beta.358 tree. Test-only; firmware src unchanged.

2026-11-18 overlap check vs TASK-1995 after beta.370 (otgw-docs, read-only; coordinator asked). gruijter's 92 [loop-stall] lines (beta.250) have two loop-task LittleFS causes, now both off the loop task: (1) state-file writes in the timer step (this task's AC1, bench 342: stalls 247-338 ms after every 6th of 40 loop-task writes, 0 for the same writes from a core-0 task), fixed by 3bd8e9112 (fsWriter, TASK-1989); (2) the 5-min write probe and the 60-s mount check in loopLittleFSStatus() (TASK-1995 AC1: [fs-slow] write-probe 558 ms 7 ms before the one 568 ms stall in step 5->20), fixed by 41a9e44ed (TASK-1995 Step B, beta.370: 0 stalls in 62 min on .61). The :x0:30/:x5:30 rhythm in gruijter's capture fits both 5-min timers (satSavePidState 5-min PID timer, SATcontrol.ino:4151; fsWriteProbe DECLARE_TIMER_MIN 5, helperStuff.ino:317), so neither fix alone explains all 92. RECOMMENDATION: narrow, do not close. AC1 holds. AC2 can be ticked on the 342 same-build experiment as the old-vs-fix for the file-write cause (coordinator's call), with 370's 62 min 0-stall bench as joint evidence shared with TASK-1995, not counted twice. AC3 (data integrity of the queued writes on .63 with SAT active: PID file every 5 min, pid_i equal after /ReBoot, sat_*.json byte-identical across a filesystem OTA) is not covered by TASK-1995; TASK-2007's .63 bench showed a save held through a filesystem OTA lands after the restore, which covers part of the last clause only. Field closure for both tasks: one gruijter capture on 370 or later (ask via 92); the expected result is near 0 [loop-stall] where beta.250 had 92 in 25.5 h.
<!-- SECTION:NOTES:END -->
