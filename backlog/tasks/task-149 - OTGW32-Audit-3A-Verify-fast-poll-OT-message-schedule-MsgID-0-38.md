---
id: TASK-149
title: 'OTGW32-Audit-3A: Verify fast-poll OT message schedule (MsgID 0-38)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:19'
updated_date: '2026-04-08 22:32'
labels:
  - audit
  - otgw32
  - phase-3
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit the fast-poll and periodic write entries in otSchedule[]: MsgID 0 status at 800ms, write entries (1,7,8,14,16,24,27,56,57) at 15s, temperature reads (17-19, 25-32, 35-36, 38) at 10s. Verify intervals match OT-Thing, write entries only fire when valueSet=true, and the schedule rotates correctly without starvation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MsgID 0 status frame is sent at least every 800ms
- [x] #2 Write entries (1,7,8,14,16,24,27,56,57) fire every 15s only when valueSet=true
- [x] #3 Temperature reads (MsgID 17-38 range) fire every 10s
- [x] #4 Schedule rotates through all entries without starvation of fast-poll MsgID 0
- [x] #5 Intervals match OT-Thing timing constants
- [x] #6 Any deviation results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Code reviewed: OTDirect.ino lines 38-232 (schedule table) and 989-1045 (scheduler loop).

**MsgID 0 (status) at 800ms**: Confirmed. OT_STATUS_INTERVAL_MS=800. Entry is index 0 in otSchedule[].

**Write entries at 15s (OT_WRITE_INTERVAL_MS=15000)**: Confirmed for MsgIDs 1,7,8,14,16,24,27,56,57. All use W_ENTRY macro (isWrite=true, valueSet=false). Guard at line 1018 ensures they only fire when valueSet=true.

**Temperature reads at 10s (OT_TEMP_INTERVAL_MS=10000)**: Confirmed for MsgIDs 17,18,19,25,26,28,29,30,31,32,35,36,38. Note: MsgID 27 (Toutside) is a W_ENTRY not R_ENTRY — correct, outside temp is written not read.

**Note**: MsgIDs 33 (exhaust temp) and 34 (boiler heat exchanger temp) are in the 60s slow-poll group, not the 10s group. This is reasonable for non-critical diagnostics.

**Schedule rotation / MsgID 0 starvation**: No starvation risk. Scheduler loop (lines 1009-1045) round-robins through all entries but each entry checks its own timestamp. MsgID 0 at 800ms will be found due on average every 800ms regardless of where the index is, since the loop scans all entries on every 100ms timer tick. Command queue (priority path) drains quickly and cannot block the schedule indefinitely.

**Intervals match OT-Thing**: STATUS_INTERVAL_MS=800 matches OT-Thing; WRITE=15s, TEMP=10s, SLOW=60s are consistent with OT-Thing design.

**Gap found**: MsgIDs 20 (Day/Time), 21 (Date), 22 (Year) are absent from the schedule. These are optional but OT-Thing polls them. Creating audit-fix task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited fast-poll entries in otSchedule[] against OT-Thing timing constants.

All verified correct:
- MsgID 0 (status) at 800ms: present and exempt from disable logic
- Write entries 1,7,8,14,16,24,27,56,57 at 15s with valueSet guard: correct
- Temperature reads 17-19, 25-32, 35-36, 38 at 10s: correct (MsgID 27 is a write, MsgIDs 33/34 are slow-poll at 60s, which is reasonable)
- Schedule rotation: round-robin with per-entry timestamp check. No starvation of MsgID 0 — it fires every 800ms regardless of index position
- Intervals match OT-Thing: STATUS=800ms, WRITE=15s, TEMP=10s, SLOW=60s

Gap found: MsgIDs 20/21/22 (Day/Time, Date, Year) absent from schedule.
Audit-fix created: TASK-180.
<!-- SECTION:FINAL_SUMMARY:END -->
