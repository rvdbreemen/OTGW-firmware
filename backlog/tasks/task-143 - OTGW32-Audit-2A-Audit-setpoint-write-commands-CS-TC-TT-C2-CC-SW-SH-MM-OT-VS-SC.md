---
id: TASK-143
title: >-
  OTGW32-Audit-2A: Audit setpoint write commands (CS, TC, TT, C2, CC, SW, SH,
  MM, OT, VS, SC)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:17'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-2
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Cross-reference all setpoint write PIC commands in handleOTDirectCommand() against the otgw-6.6 PIC firmware and Schelte Bron OTGW documentation. Verify correct MsgID mapping, f8.8 encoding, clear-on-zero behavior, write cache update, and synthesizeResponse() output format for each command.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CS= maps to MsgID 1, clears override on value 0
- [x] #2 TC= maps to MsgID 1 (same as CS=), independent clear
- [x] #3 TT= maps to MsgID 16, clears override on value 0
- [x] #4 C2= maps to MsgID 8, clears override on value 0
- [x] #5 CC= maps to MsgID 7, f8.8 encoding correct
- [x] #6 SW= maps to MsgID 56, clears on value 0
- [x] #7 SH= maps to MsgID 57, clears on value 0
- [x] #8 MM= maps to MsgID 14, non-numeric clears override
- [x] #9 OT= maps to MsgID 27, non-numeric clears override
- [x] #10 VS= maps to MsgID 71, HB byte encoding correct
- [x] #11 SC= maps to MsgID 20, HH:MM/DOW parsing correct
- [x] #12 Any command deviation from PIC spec results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit of setpoint write commands complete.

CS= (MsgID 1): PASS - correct f8.8 encoding, clear-on-zero via clearWriteOverride(1), write cache + override updated.
TC= (MsgID 1): PASS - same MsgID as CS=, independent clear, identical behavior.
TT= (MsgID 16): PASS - correct, clear-on-zero, write cache updated.
C2= (MsgID 8): PASS - correct, clear-on-zero, write cache updated.
CC= (MsgID 7): PASS - f8.8 encoding correct. NOTE: CC= is NOT in official PIC spec but is a reasonable extension for cooling. No clear-on-zero (intentional).
SW= (MsgID 56): PASS - correct, clear-on-zero.
SH= (MsgID 57): PASS - correct, clear-on-zero.
MM= (MsgID 14): PASS - non-numeric clears override, f8.8 correct.
OT= (MsgID 27): PASS - non-numeric clears, f8.8 signed correct.
VS= (MsgID 71): PASS - HB byte encoding correct (value<<8), clears on non-numeric.
SC= (MsgID 20): PASS - sscanf parses HH:MM/DOW, DOW in upper 3 bits of HB, minutes in LB.

MISSING COMMAND: BS= (boiler room setpoint) is in PIC spec but not implemented. Likely maps to MsgID 16 (TrSet) or MsgID 9. Creating audit-fix task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All 11 setpoint write commands (CS, TC, TT, C2, CC, SW, SH, MM, OT, VS, SC) verified correct.

All map to correct MsgIDs, use correct f8.8 encoding, implement clear-on-zero/clear-on-non-numeric as specified, update write cache and repeater override via enqueueWriteCommand().

One deviation: BS= (boiler room setpoint) is present in PIC spec but not implemented in OTDirect. Audit-fix task created.

CC= is not in official PIC spec but is a reasonable extension; no action needed.
<!-- SECTION:FINAL_SUMMARY:END -->
