---
id: TASK-1908
title: 'OT-Direct: SC= accepts times and days the PIC refuses'
status: To Do
assignee: []
created_date: '2026-11-13T18:50:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 974000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1851 delta parity check (2026-11-13, verified in both sources). PIC 6.8 SetClock (gateway.asm:4331-4370): hours above 23 -> OR, minutes above 59 -> OR, a missing ':' or '/' -> SE, day of week must be 1-7 (0 or above 7 -> BV), trailing characters -> SE. OT-Direct (OTDirect.ino:3490-3498): siscanf("%d:%d/%d") >= 2 accepts SC=12:30 without a day and any range, masks the bits (:3494) and then serves that value as MsgID 20 to the thermostat. Not covered by TASK-1804 (silent parse failure only), TASK-1849 (reply format only) or TASK-1859 (f8.8 values).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real SC= branch: SC=24:00/1 -> OR, SC=12:60/1 -> OR, SC=12:30 -> SE, SC=12:30/0 -> BV, SC=12:30/3x -> SE, and otSCLastCommandMs stays unchanged in each case; red on the current code, green on the fix
- [ ] #2 SC=12:30/3 still stores and answers as today (regression)
<!-- AC:END -->
