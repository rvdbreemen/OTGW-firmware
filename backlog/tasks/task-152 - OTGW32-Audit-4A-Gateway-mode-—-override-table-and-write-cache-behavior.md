---
id: TASK-152
title: 'OTGW32-Audit-4A: Gateway mode — override table and write cache behavior'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:19'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-4
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit gateway mode (GW=1): thermostat frames that match an active override entry in otOverrides[] must have their data value replaced before forwarding to the boiler. Verify that inactive overrides pass frames through unmodified, and that the write cache (periodic 15s refresh) correctly keeps boiler values alive in gateway mode.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Active override replaces thermostat data value for matching MsgIDs (1,7,8,14,16,56,57)
- [x] #2 Inactive override passes thermostat frame through unmodified
- [x] #3 Write cache refreshes overridden values to boiler every 15s
- [x] #4 Clearing an override (value=0) stops the periodic write
- [x] #5 Frame forwarding to boiler is not delayed by override processing
- [x] #6 Any behavioral issue results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

- `otOverrides[]` array (line 272): 7 entries covering MsgIDs 1,7,8,14,16,56,57 — matches spec exactly
- `applyOverrides()` (line 374): replaces lower 16 bits of frame, recalculates parity, sets modified=true — correct
- Inactive overrides (active=false): applyOverrides skips them, frame passes unmodified — correct
- When modified: logs original as T-frame and sends modified as R-frame to boiler — correct
- Write cache: `W_ENTRY()` in otSchedule (lines 114-122) for all 7 MsgIDs at OT_WRITE_INTERVAL_MS=15000ms — confirmed
- `enqueueWriteCommand()` (line 1260): calls both updateWriteCache() AND setOverride() together — correct
- Clearing override: `clearWriteOverride()` (line 1311) sets valueSet=false (stops periodic write) AND calls clearOverride() (deactivates repeater) — correct
- Frame forwarding: override processing is synchronous before async send, no delay path — correct
- No behavioral gaps found.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Gateway mode override and write-cache audit: all ACs pass.

- otOverrides[] covers the correct 7 MsgIDs (1,7,8,14,16,56,57)
- applyOverrides() replaces lower 16 bits, recalculates parity, logs T-frame before forwarding modified R-frame
- Inactive overrides: frame passes unmodified — confirmed
- Write cache: W_ENTRY schedule entries at OT_WRITE_INTERVAL_MS (15000ms) for all 7 writable MsgIDs
- enqueueWriteCommand() atomically updates both write cache and repeater override
- clearWriteOverride() correctly sets valueSet=false (stops periodic write) and deactivates repeater override
- No behavioral gaps found. No audit-fix tasks required.
<!-- SECTION:FINAL_SUMMARY:END -->
