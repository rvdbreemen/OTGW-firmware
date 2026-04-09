---
id: TASK-147
title: >-
  OTGW32-Audit-2E: Audit schedule and override commands (AA, DA, PM, SR, CR, UI,
  KI, RM, CM, TP)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:18'
updated_date: '2026-04-08 22:34'
labels:
  - audit
  - otgw32
  - phase-2
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify schedule management, response override, unknown-ID handling, response modifier, and thermostat parameter commands against PIC specification. Key areas: AA= re-enable + clearUnknownCount(), DA= disable, PM= force-immediate or one-shot READ, SR= response override table, CR= clear, UI= unknown ID + schedule disable, KI= known again + schedule re-enable, RM= response modifier, CM= clear modifier, TP= TSP/FHB read/write with MsgID validation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 AA= re-enables disabled schedule entry and resets 3-strike counter
- [x] #2 DA= disables a schedule entry
- [x] #3 PM= forces immediate send for scheduled entries; one-shot READ for unscheduled MsgIDs
- [x] #4 SR= stores override in response override table, activates for slave response
- [x] #5 CR= clears response override entry
- [x] #6 UI= adds MsgID to unknown list and disables from schedule
- [x] #7 KI= removes MsgID from unknown list, re-enables in schedule
- [x] #8 RM= stores response modifier for boiler→thermostat data modification
- [x] #9 CM= clears response modifier
- [x] #10 TP= validates MsgIDs (11/13/89/91/106/108), rejects writes to FHB entries
- [x] #11 Any deviation from PIC spec results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit of schedule/override commands complete.

AA= (re-enable): PASS - sets disabled=false, calls clearUnknownCount() to reset 3-strike counter. Returns NF if not in schedule.
DA= (disable): PASS - sets disabled=true. Returns NF if not in schedule.
PM= (priority message): PASS - for scheduled entries sets lastSentMs=0 and re-enables; for unscheduled enqueues one-shot READ_DATA.
SR= (response override): PASS - stores in otResponseOverrides[] table, activated for slave response. Finds existing slot or free slot. Returns NS if table full (max 16).
CR= (clear override): PASS - clears active flag. Returns NF if not found.
UI= (mark unknown): PASS - adds to otUnknownIds[], disables from schedule. Returns NS if table full (max 16).
KI= (mark known): PASS - removes from otUnknownIds[] via swap-with-last, calls clearUnknownCount(), re-enables in schedule.
RM= (response modifier): PASS - stored in otResponseModifiers[] (max 8 slots). Applied via applyResponseModifiers() in gateway mode before forwarding to thermostat.
CM= (clear modifier): PASS - clears active flag in table.
TP= (TSP/FHB): PASS - validates MsgIDs (11/13/89/91/106/108), rejects writes to FHB entries (13/91/108), builds READ_DATA or WRITE_DATA with index in HB.

All 11 ACs verified.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All 10 schedule/override commands (AA, DA, PM, SR, CR, UI, KI, RM, CM, TP) verified correct.

- AA/DA/PM correctly manage schedule enable/disable and 3-strike counter reset.
- SR/CR/UI/KI table management correct with NF/NS error responses.
- RM/CM response modifier path applied only in gateway mode before thermostat forwarding.
- TP correctly validates the 6 valid MsgIDs and rejects writes to FHB (read-only) entries.

No deviations from PIC spec.
<!-- SECTION:FINAL_SUMMARY:END -->
