---
id: TASK-151
title: 'OTGW32-Audit-3C: Audit auto-disable (3-strike UNKNOWN_DATA_ID) logic'
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
Verify the auto-disable mechanism: when a boiler responds UNKNOWN_DATA_ID to a scheduled MsgID three times, the entry is disabled. Verify the 3-strike counter, the disable trigger, AA= re-enable + clearUnknownCount() reset, and that MsgID 0 (status) is exempt from auto-disable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 3 consecutive UNKNOWN_DATA_ID responses disable a schedule entry
- [x] #2 MsgID 0 is never auto-disabled regardless of boiler response
- [x] #3 AA= re-enables a disabled entry and resets the strike counter
- [x] #4 UI= manually disables an entry and adds to unknown-ID list
- [x] #5 KI= removes from unknown-ID list and re-enables in schedule
- [ ] #6 Any logic error results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Code reviewed: OTDirect.ino lines 348-369 (counter storage), 912-934 (auto-disable in handleMasterResponse), 2087-2225 (AA=, UI=, KI= commands).

**3-strike disable logic (lines 912-930)**: On UNKNOWN_DATA_ID response: incUnknownCount(respMsgId). If count >= 3, scan schedule and set disabled=true. Uses 2-bit packed counter in otUnknownCounters[32] (128 MsgIDs × 2 bits). Counter is capped at 3 by the guard in incUnknownCount(). Confirmed correct.

**MsgID 0 exemption (line 919)**: Guard `if (respMsgId != 0)` wraps both the increment and the disable trigger. MsgID 0 status frame is never auto-disabled regardless of boiler response. Confirmed correct.

**Successful response resets counter (lines 931-933)**: On non-UNKNOWN_DATA_ID response for non-zero MsgID, clearUnknownCount(respMsgId) is called. This ensures transient UNKNOWN responses don't accumulate across separate issues. Confirmed correct.

**AA= command (lines 2090-2106)**: Re-enables disabled schedule entry AND calls clearUnknownCount(msgId). So counter is reset to 0 before boiler is polled again, preventing immediate re-disable. Bounds check: rejects MsgID 0 (processOT("BV")) and > 127. Confirmed correct.

**UI= command (lines 2195-2209)**: Adds msgId to otUnknownIds[] list (max 16 entries) and disables from schedule. This is a MANUAL unknown-ID override for the thermostat path (gateway responds UNKNOWN_DATAID to thermostat without forwarding to boiler). Does NOT call clearUnknownCount — this is fine since UI= is a manual user directive, not related to auto-disable. Confirmed correct.

**KI= command (lines 2210-2225)**: Removes from otUnknownIds[] via swap-with-last, calls clearUnknownCount(msgId), and re-enables in schedule. Correct restore behavior.

**No logic errors found.** All 5 behaviors are correctly implemented.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited the 3-strike UNKNOWN_DATA_ID auto-disable logic in OTDirect.ino.

All mechanisms verified correct:
- 3 consecutive UNKNOWN_DATA_ID responses disable the schedule entry (handleMasterResponse, lines 912-930). Counter uses 2-bit packed storage, capped at 3.
- MsgID 0 is exempt: explicit `if (respMsgId != 0)` guard on both increment and disable trigger
- Successful non-zero response resets the counter via clearUnknownCount()
- AA= re-enables disabled entry AND calls clearUnknownCount() so counter resets to 0 before next poll — prevents immediate re-disable
- UI= adds to unknown-ID list (thermostat path intercept) and disables from schedule. Correct manual override behavior
- KI= removes from unknown-ID list, clears strike counter, and re-enables in schedule. Full restore

No logic errors or gaps found. AC 6 not applicable (no fix needed).
<!-- SECTION:FINAL_SUMMARY:END -->
