---
id: TASK-1867
title: >-
  SAT pressure-drop alarm fires on a normal cool-down (gruijter 2026-11-12 07:22
  and 2026-11-11 21:45, beta.182)
status: To Do
assignee: []
created_date: '2026-11-12T12:09:00Z'
updated_date: '2026-11-12T12:15:00Z'
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
- [ ] #1 gruijter's MsgID 18 series of 11-12 Nov is replayed against the current alarm code (a host test on the real code) and reproduces both alarms
- [ ] #2 The replay shows whether these alarms are the known pump-stop residual accepted in TASK-1579 / entry 262, or a different trigger, and the finding is written in the task notes
- [ ] #3 If a change is proposed, it is put to Sergeant D with the thermo-nova citations before any implementation; otherwise the task is closed with the evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sources (comms, 2026-11-12): the analysis output is in C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\9604259c-3cd4-4397-a002-3ea5ca73cb77\scratchpad\gruijter-20261112\out\. Verified there: sat_narr_0620.txt lines 28 and 30 hold the alarm ('SAT: PRESSURE ALARM (smoothed=1.81 drop=1.758 bar/hr)' at 07:22:54.990, cleared 07:25:24.993). NOT verified: the MsgID 18 values around the 07:22 alarm (2.01-2.03 hot, 1.70 at 07:14, 2.00 at 07:29). win_0620_1210.txt holds only MsgID 17, 56 and 57 RD-ACKs, and win_all.txt covers about 00:54-01:33 only. Take those values from the raw upload (20261111-213113-upload.zip) for the replay. What win_all.txt does show: MsgID 18 at 1.68 bar, then 1.95/1.94/1.95 at 01:08:45-01:18:35 while the pump ran after the reboot, then 1.68 again from 01:23. That is a pump-step pattern of about 0.27 bar, the same kind of step as the pump-stop residual in TASK-1579 / entry 262.

Update (comms, 2026-11-12, checked first-hand): raw source C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\9604259c-3cd4-4397-a002-3ea5ca73cb77\scratchpad\gruijter-20261112\work\x\csv\otframes.csv (MsgID 18 READ-ACK every ~5 min). 06:30:34 1.68, 06:35:29 1.98 (pump on), 07:04:39 1.98, 07:09:33 1.97, 07:14:31 1.70, 07:19:21 1.69, 07:24:11 1.69, 07:29:01 2.00 (pump on again), 07:33:53 1.97. The pressure is ~1.68 with the pump off and ~2.0 with it running, a ~0.3 bar pump step; the 07:22 alarm follows the heating-off at 07:09:54 (per the coordinator's check). So this is NOT thermal contraction, as the title says: it is the pump-stop residual accepted in TASK-1579 / entry 262. The task question (known residual or different trigger) is mostly answered: same trigger. The replay AC stays, to confirm it on the real code. The title wording 'normal cool-down' is the first reading and is kept for traceability. Public correction to gruijter: msg 1570411xxx (see channel).

Correction to the note above: the public correction to gruijter is #alpha-testing msg 1570411600530243658 (https://discord.com/channels/812969634638725140/1514720723980259460/1570411600530243658).
<!-- SECTION:NOTES:END -->
