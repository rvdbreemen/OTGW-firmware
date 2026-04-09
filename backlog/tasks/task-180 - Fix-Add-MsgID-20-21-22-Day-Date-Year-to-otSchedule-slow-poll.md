---
id: TASK-180
title: 'Fix: Add MsgID 20/21/22 (Day, Date, Year) to otSchedule[] slow-poll'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:31'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit TASK-149 found that MsgIDs 20 (Day of week/time), 21 (Date), and 22 (Year) are absent from otSchedule[] in OTDirect.ino. OT-Thing polls these for time-sync purposes. They should be added as R_ENTRY at 60s slow-poll interval so the firmware fetches date/time info from the boiler.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add R_ENTRY(20, OT_SLOW_INTERVAL_MS) to otSchedule[] in the slow-poll section
- [x] #2 Add R_ENTRY(21, OT_SLOW_INTERVAL_MS) to otSchedule[] in the slow-poll section
- [x] #3 Add R_ENTRY(22, OT_SLOW_INTERVAL_MS) to otSchedule[] in the slow-poll section
- [x] #4 Entries auto-disable correctly via 3-strike logic if boiler returns UNKNOWN_DATA_ID
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added MsgID 20 (Day/Time), 21 (Date), 22 (Year) to the otSchedule[] slow-poll table.

Inserted R_ENTRY(20), R_ENTRY(21), R_ENTRY(22) with OT_SLOW_INTERVAL_MS (60s) into the slow-poll section, near MsgID 9 and 15. These are read-only boiler responses per the OpenTherm spec and auto-disable via existing 3-strike UNKNOWN_DATA_ID logic if the boiler does not support them.
<!-- SECTION:FINAL_SUMMARY:END -->
