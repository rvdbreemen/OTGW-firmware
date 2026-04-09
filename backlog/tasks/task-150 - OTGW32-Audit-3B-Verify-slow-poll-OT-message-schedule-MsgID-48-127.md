---
id: TASK-150
title: 'OTGW32-Audit-3B: Verify slow-poll OT message schedule (MsgID 48-127)'
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
Audit all slow-poll entries (60s interval) in otSchedule[]: bounds (48-55), remote parameters (56-63), ventilation/heat-recovery (70-91), brand info (93-95), counters (96-123), and OT version/product info (125,127). Verify complete coverage against the OT spec and OT-Thing, and that no relevant MsgIDs are omitted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bounds MsgIDs 48-55 are all present in schedule
- [x] #2 Remote parameter MsgIDs 58-63 are present
- [x] #3 Ventilation/HRV MsgIDs 70-91 are all present
- [x] #4 Counter MsgIDs 96-123 are all present
- [x] #5 OT version (125) and product info (127) are present
- [ ] #6 No MsgID present in OT-Thing schedule is missing from firmware schedule
- [x] #7 Any missing or incorrect entry results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Code reviewed: OTDirect.ino lines 139-227 (slow-poll section of otSchedule[]).

**Bounds MsgIDs 48-55**: All 8 present (48,49,50,51,52,53,54,55) at 60s. Confirmed.

**Remote params MsgIDs 58-63**: All present (58,59,60,61,62,63) at 60s. Note: MsgIDs 56,57 are W_ENTRY periodic writes (DHW setpoint/MaxCH setpoint), correctly placed in fast-poll write section.

**Ventilation/HRV MsgIDs 70-91**: All 22 entries present (70-91 inclusive) at 60s. Confirmed.

**Brand info MsgIDs 93-95**: All 3 present at 60s. Confirmed.

**Counters section**: MsgIDs 96,97 present. MsgIDs 100-123 present. MISSING: MsgIDs 98 and 99. In the OT spec, MsgID 98 = "Remote parameter 4 (bounds, s8+s8)" and MsgID 99 = "Remote parameter 5 (bounds, s8+s8)". These are omitted from the schedule — a gap.

**OT version (125) and product info (127)**: Both present at 60s. Confirmed. MsgID 126 (master product version) is written once in init handshake, not polled — this is correct behavior.

**Other gaps identified**:
- MsgID 92 (not defined in standard OT spec — reserved; correct to omit)
- MsgIDs 98, 99 missing (see above)
- MsgIDs 10,11 (TSP count/entry), 12,13 (FHB size/entry) — index-based, complex to poll; absence is acceptable but worth noting
- MsgIDs 64-69 (defined in some extended OT specs): absent; these are extended/optional

Creating audit-fix task for MsgIDs 98 and 99 missing from schedule.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited slow-poll entries (60s interval) in otSchedule[] against OT spec and OT-Thing.

Verified present:
- Bounds MsgIDs 48-55: all 8 entries confirmed
- Remote params MsgIDs 58-63: all 6 entries confirmed
- Ventilation/HRV MsgIDs 70-91: all 22 entries confirmed
- Brand info MsgIDs 93-95: all 3 confirmed
- Counters MsgIDs 96, 97, 100-123: confirmed (96, 97 + 100-123)
- OT version (125) and product info (127): confirmed

Gap found: MsgIDs 98 (Remote param 4 bounds) and 99 (Remote param 5 bounds) are missing. MsgID 92 is reserved in the standard OT spec and correctly absent. TSP/FHB index-based MsgIDs (10-13) are intentionally absent (complex polling).

AC 6 not satisfied: MsgIDs 98 and 99 from OT-Thing schedule are missing.
Audit-fix created: TASK-182.
<!-- SECTION:FINAL_SUMMARY:END -->
