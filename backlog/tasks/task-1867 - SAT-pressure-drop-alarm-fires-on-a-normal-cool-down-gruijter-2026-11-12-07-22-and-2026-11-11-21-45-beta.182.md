---
id: TASK-1867
title: >-
  SAT pressure-drop alarm fires on a normal cool-down (gruijter 2026-11-12 07:22
  and 2026-11-11 21:45, beta.182)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T12:09:00Z'
updated_date: '2026-11-16T00:14:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 943000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From gruijter's capture of 12 Nov (#alpha-testing msg https://discord.com/channels/812969634638725140/1514720723980259460/1570395420667023420, upload 20261111-213113-upload.zip). The SAT pressure alarm fired 07:22:54-07:25:24 ('PRESSURE ALARM (smoothed=1.81 drop=1.758 bar/hr)', SATcontrol.ino:5440 on origin/dev) while, per the analysis, MsgID 18 went about 2.0 bar hot -> 1.70 at 07:14 -> 2.0 at 07:29 (5-min samples): normal thermal contraction, not a leak. Also on 11 Nov 21:45. Context: TASK-1579 (Done) and TASK-1511 kept the alarm at thermo-nova parity and accepted a residual of pump-stop alarms (entry 262: the 600 s flame-off suspension did not remove them). thermo-nova pressure health: binary_sensor.py:24-27 (PRESSURE_DROP_RATE_SETTLE_SECONDS 600, PRESSURE_EMA_ALPHA 0.05, PRESSURE_PROBLEM_CONFIRMATION_SECONDS 120, PRESSURE_DROP_RATE_MIN_WINDOW_SECONDS 300), class SatPressureHealthSensor at :200. Any change to the alarm rule is SAT behaviour and goes to Sergeant D first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 gruijter's MsgID 18 series of 11-12 Nov is replayed against the current alarm code (a host test on the real code) and reproduces both alarms
- [x] #2 The replay shows whether these alarms are the known pump-stop residual accepted in TASK-1579 / entry 262, or a different trigger, and the finding is written in the task notes
- [x] #3 If a change is proposed, it is put to Sergeant D with the thermo-nova citations before any implementation; otherwise the task is closed with the evidence
- [x] #4 Sergeant D's B (entry 362) is implemented: satUpdatePressure() records no pressure samples while the drop rate is suspended (600 s after a CH-mode change, PRESSURE_DROP_RATE_SETTLE_SECONDS at thermo-nova binary_sensor.py:25)
- [x] #5 The replay of gruijter's series (tests/test_sat_pressure_alarm_replay.py) is red on the old code (the three field alarms reproduce) and green on the fix (none of them fires)
- [x] #6 A real pressure loss that continues after the suspension still raises the alarm on the fix (host test with a synthetic series through the real code)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sources (comms, 2026-11-12): the analysis output is in C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\9604259c-3cd4-4397-a002-3ea5ca73cb77\scratchpad\gruijter-20261112\out\. Verified there: sat_narr_0620.txt lines 28 and 30 hold the alarm ('SAT: PRESSURE ALARM (smoothed=1.81 drop=1.758 bar/hr)' at 07:22:54.990, cleared 07:25:24.993). NOT verified: the MsgID 18 values around the 07:22 alarm (2.01-2.03 hot, 1.70 at 07:14, 2.00 at 07:29). win_0620_1210.txt holds only MsgID 17, 56 and 57 RD-ACKs, and win_all.txt covers about 00:54-01:33 only. Take those values from the raw upload (20261111-213113-upload.zip) for the replay. What win_all.txt does show: MsgID 18 at 1.68 bar, then 1.95/1.94/1.95 at 01:08:45-01:18:35 while the pump ran after the reboot, then 1.68 again from 01:23. That is a pump-step pattern of about 0.27 bar, the same kind of step as the pump-stop residual in TASK-1579 / entry 262.

Update (comms, 2026-11-12, checked first-hand): raw source C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\9604259c-3cd4-4397-a002-3ea5ca73cb77\scratchpad\gruijter-20261112\work\x\csv\otframes.csv (MsgID 18 READ-ACK every ~5 min). 06:30:34 1.68, 06:35:29 1.98 (pump on), 07:04:39 1.98, 07:09:33 1.97, 07:14:31 1.70, 07:19:21 1.69, 07:24:11 1.69, 07:29:01 2.00 (pump on again), 07:33:53 1.97. The pressure is ~1.68 with the pump off and ~2.0 with it running, a ~0.3 bar pump step; the 07:22 alarm follows the heating-off at 07:09:54 (per the coordinator's check). So this is NOT thermal contraction, as the title says: it is the pump-stop residual accepted in TASK-1579 / entry 262. The task question (known residual or different trigger) is mostly answered: same trigger. The replay AC stays, to confirm it on the real code. The title wording 'normal cool-down' is the first reading and is kept for traceability. Public correction to gruijter: msg 1570411xxx (see channel).

Correction to the note above: the public correction to gruijter is #alpha-testing msg 1570411600530243658 (https://discord.com/channels/812969634638725140/1514720723980259460/1570411600530243658).

2026-11-14 coordinator (subagent, verified: python -m pytest -q tests/test_sat_pressure_alarm_replay.py -> 4 passed in 13.67s): replay of the REAL satUpdatePressure() (sliced from SATcontrol.ino, harness of test_sat_pressure_health.py) on gruijter's MsgID 0/18 series (fixture tests/fixtures/sat_pressure_gruijter_20261111.csv, 282 rows, timestamps + values only). AC1: 07:22:54 alarm exact (smoothed 1.813, drop 1.765, cleared 07:25:24, log 1.81/1.758); 01:31:24 onset/drop/clear match (smoothed 0.024 low, EMA seed before the capture); 21:45 only with an assumed 2.0 bar before the first captured read (labelled as assumption in the test). AC2: same trigger as the pump-stop residual of TASK-1579 / entry 262: the CH-mode bit falls -> ring cleared + 600 s suspension (SATcontrol.ino:5699-5709), the ring keeps recording (:5726-5735), the pump-stop step (~0.27 bar) lands 240 s later inside the suspension; at suspension end (:5745) the least-squares slope over pre/post step reads ~2.2 bar/h > 0.3, confirmed after 120 s (:5796, :5807) = 720 s after the CH bit. Not thermal contraction (title wording is the first reading). A flame-off suspension would not help: the step comes 870 s after flame-off (07:22) and there was no flame at all before 01:31. AC3 open: any change (e.g. drop samples taken during the suspension) deviates from thermo-nova and goes to Sergeant D as Q-1867a.

Q-1867a decided 2026-11-15 by the coordinator under Robert's directive (CLI) 'Do not wait for Sergeant D. Finish everything that can be finished.'; Sergeant D may overrule, his answer later becomes a follow-up task; asked in the 18-question batch in #dev-sat-mqtt (item 9, https://discord.com/channels/812969634638725140/1105556725714649128/1571633456381694074): option B, discard the pressure samples recorded during the 600 s settle window (thermo-nova binary_sensor.py:25 PRESSURE_DROP_RATE_SETTLE_SECONDS 600, opened 2026-11-15). Unblocked.

AC3: put to Sergeant D in the 18-question batch (item 9) and answered B on 2026-11-15 (entry 362, https://discord.com/channels/812969634638725140/1105556725714649128/1571637957230862506): discard the samples recorded during the 600 s settle window. Implementation ACs #4-#6 added; picked up by 92.

Implementation (92, impl agent, verified by 92 2026-11-16): satUpdatePressure() records no samples while the drop rate is suspended (entry 362 B), and after a settle the first fit waits until the fresh samples span 600 s (choice A, coordinator 2026-11-15; thermo-nova keeps recording during its settle, binary_sensor.py:340-342, SETTLE 600 :25, MIN_WINDOW 300 :28, so its first fit spans ~600 s; asked as Q-1867b, https://discord.com/channels/812969634638725140/1105556725714649128/1571662081084629088). Constants PRESS_SETTLE_MS/PRESS_MIN_SPAN_MS, flag _press_postSettle.

Evidence (run by 92): tests/test_sat_pressure_alarm_replay.py PressureAlarmReplayFixed + SyntheticLoss with OTGW_SAT_INO = origin/dev SATcontrol.ino: Ran 5, FAILED (failures=4) (field alarms 01:31/07:22 still fire, assumed-seed 21:45 fires, leak-after-CH timing). Fix, whole file: Ran 9, OK. Alarm onsets: gruijter replay old 01:31:24.99 + 07:22:54.99, B-only adds 10:28:24.99, A none; synthetic 0.5 bar/h leak with a CH change at 120 s: old 840 s, B-only 1140 s, A 1440 s (+600 s vs old, the cost of A); without a CH change 420 s on all. The B-only red: the 10:28:24 assertion fails on the B-only copy (impl agent). run_tests test_sat_pressure_alarm_replay/test_sat_pressure_health/test_sat_ch_signals PASS; evaluate --quick 0 failed. Note: the thermo-nova oracle in test_sat_pressure_health.py now has drop_settle_samples=True (our deliberate deviation, entry 362 + A); False gives thermo-nova as written.

Choice A confirmed by Robert (maintainer): reply 'A' to Q-1867b, 2026-11-15 23:11:14 UTC, https://discord.com/channels/812969634638725140/1105556725714649128/1571663900985000038; recorded as entry 372.

Bench smoke (09, .61, beta.331+e4dbf70, coordinator: smoke only): sat/pressure_alarm false at every publish and in /api/v2/sat at every step incl. a SAT off/on toggle. Limit: no pressure sensor on that bench (pressure 0.00, pressure_health OFF), so this only shows no alarm out of nothing; the alarm logic is proven by the host replay.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
False SAT pressure-drop alarms after a pump stop (gruijter, beta.182) fixed: readings of the 600 s settle window after a CH-mode change are not recorded (entry 362, B) and the first fit after a settle waits for 600 s of fresh readings (choice A, entry 372, Robert). AC1-3: replay of gruijter's MsgID 18 series reproduces the 01:31 and 07:22 alarms on the old code (tests/test_sat_pressure_alarm_replay.py, real host-compiled satUpdatePressure). AC4-6: fix tests FAILED (failures=4) on origin/dev code, Ran 9 OK on the fix; 0.5 bar/h leak after a CH change alarms at 1440 s (old 840 s), without a CH change 420 s on both; evaluate --quick 0 failed. Bench smoke 09 on beta.331+e4dbf70: no alarm (no pressure sensor on that bench). Not verified: a real pressure loss on hardware. Commit 2217f8f3f (beta.332).
<!-- SECTION:FINAL_SUMMARY:END -->
