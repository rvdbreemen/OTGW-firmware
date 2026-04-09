---
id: TASK-182
title: 'Fix: Add MsgID 98 and 99 (remote parameter bounds) to otSchedule[] slow-poll'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:32'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit TASK-150 found that MsgIDs 98 (Remote parameter 4 upper/lower bounds, s8+s8) and 99 (Remote parameter 5 upper/lower bounds, s8+s8) are absent from otSchedule[] in OTDirect.ino. MsgIDs 48-55 cover the first set of bounds and 58-63 cover the remote parameter values, but 98-99 are skipped. They should be added as R_ENTRY at 60s slow-poll interval alongside the other bounds entries.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add R_ENTRY(98, OT_SLOW_INTERVAL_MS) to otSchedule[] in the bounds/slow-poll section
- [x] #2 Add R_ENTRY(99, OT_SLOW_INTERVAL_MS) to otSchedule[] in the bounds/slow-poll section
- [x] #3 Entries auto-disable correctly via 3-strike logic if boiler returns UNKNOWN_DATA_ID
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added MsgID 98 (remote parameter 4 bounds s8+s8) and MsgID 99 (remote parameter 5 bounds s8+s8) to the otSchedule[] slow-poll table.

Inserted R_ENTRY(98) and R_ENTRY(99) with OT_SLOW_INTERVAL_MS between MsgID 97 and 100 in the counters/special section. These are read-only boiler bounds registers and auto-disable if not supported.
<!-- SECTION:FINAL_SUMMARY:END -->
