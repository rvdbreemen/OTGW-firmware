---
id: TASK-1908
title: 'OT-Direct: SC= accepts times and days the PIC refuses'
status: To Do
assignee: []
created_date: '2026-11-13T18:50:00Z'
updated_date: '2026-11-15T15:07:00Z'
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
- [x] #2 SC=12:30/3 still stores and answers as today (regression)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 16:00-16:05 bench 09 on .61 (OT-Direct), beta.320+5b9efcc (sha256 ino ec7f313f…): REST commands with telnet 'OTD: cmd' + the event_report text: SC=24:00/1 -> OR ('outside of the allowed range'), SC=12:60/1 -> OR, SC=12:30 -> SE ('unexpected character or was incomplete'), SC=12:30/0 -> BV ('data value that is not allowed'), SC=12:30/3x -> SE, SC=12:30/3 -> 'SC: 12:30/3' (AC2 regression: still answers; the stored value only feeds the thermostat's MsgID 20 answer, not visible on .61 without a thermostat). NOT seen: 'the NTP SC= still lands' - ntpsendtime was off on .61; switched on for 75 s, no SC= on telnet, switched off again (75 s is probably shorter than the send interval; re-check if 95 gives the interval). Settings diff empty afterwards. Evidence: 09 scratchpad b320/sc (telnet.raw local, tel-ntp.raw).
<!-- SECTION:NOTES:END -->
