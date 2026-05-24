---
id: TASK-685
title: >-
  Revise Unknown-Data-Id logging: drop TASK-684 suppression, keep plain-English
  direction-aware text on every emit
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 06:46'
updated_date: '2026-05-24 06:53'
labels:
  - diagnostics
  - beta.20
  - log-cleanup
  - opentherm
  - revision
dependencies: []
references:
  - TASK-684 (the one being revised)
  - OTGW_1.6_beta_20.txt (crashevans)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-684 introduced a 'log once per (msgID, direction), suppress repeats on telnet' behaviour for slave Unknown-Data-Id responses. That assumption was wrong: telnet logging is NOT always on — a tester who connects 5 minutes after boot sees zero Unknown-Data-Id lines and assumes the boiler is fine, missing the diagnostic signal entirely.

Revise the behaviour:
- Remove the suppression. Every slave Unknown-Data-Id frame continues to emit on telnet as before (plus WebSocket OT Monitor, unchanged).
- KEEP the plain-English direction-aware wording introduced by TASK-684, but emit it on EVERY occurrence instead of once. Approach: append a short plain-English suffix to the existing AddLog-built line in processOT(), so each Unknown-Data-Id line carries its own self-describing tail without doubling the line count.
- KEEP the per-msgID, per-direction bitmap that records which msgIDs the boiler has responded to with Unknown-Data-Id. It becomes the source of truth for TASK-686 (REST/MQTT/WebUI surfacing), so no functional change — only its CONSUMERS change.
- KEEP the lastMasterWasWrite bitmap, since direction tracking is still required for the suffix wording.

Read direction wording: '(boiler does not implement)'
Write direction wording: '(boiler rejected write)'

Memory: no net change vs TASK-684 (96 bytes static RAM in processOT()).

References:
- Original TASK-684 (the one being revised)
- crashevans's beta.20 telnet log: 7-8 unsupported msgIDs, ~145 Unknown-Data-Id lines / 10 min
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Inside processOT(), the OTGWDebugTf calls that emit the once-per-(id, direction) plain-English line are removed. The bitmaps unknownLoggedRead and unknownLoggedWrite are kept and continue to be set whenever Unknown-Data-Id is observed, but no longer drive suppression.
- [x] #2 The suppressTelnetForRepeat flag and the gated OTGWDebugT call are removed. Telnet emission of the raw OT-bus line for Unknown-Data-Id is unconditional, exactly as it was before TASK-684.
- [x] #3 The existing OT-bus log line gains a short direction-aware plain-English suffix on every Unknown-Data-Id frame: '(boiler does not implement)' for read direction, '(boiler rejected write)' for write direction. The suffix is appended via AddLog in processOT() so the same string reaches both telnet (OTGWDebugT) and WebSocket OT Monitor (sendLogToWebSocket).
- [x] #4 lastMasterWasWrite is preserved and continues to be updated on every master frame so the suffix-wording branch picks the correct direction.
- [x] #5 python build.py --firmware exits 0.
- [x] #6 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in commit de08130b:
- Removed the OTGWDebugTf once-per-(id, direction) emit lines.
- Removed suppressTelnetForRepeat flag and the gated OTGWDebugT call.
- Added the direction-aware plain-English suffix via AddLog inside processOT, so the same string reaches both telnet and WebSocket via ot_log_buffer.
- Bitmaps unknownLoggedRead and unknownLoggedWrite continue to be populated (idempotent set on every Unknown-Data-Id observation) so TASK-686 can consume them.
- lastMasterWasWrite continues to track the most-recent master direction per id.

Build: python build.py --firmware -> exit 0 (1.6.0-beta.20+b4ee52c).
Evaluator: python evaluate.py --quick -> 34 / 0 / 0 (100%).

Task-687 (HA discovery suppression, milestone 2.1.0) was created on this branch by mistake; the creation commit was reverted here (a5af0e9b) and the same content cherry-picked onto a dedicated feature/milestone-2.1.0 branch so it lives with its milestone and does not ride PR #640 into dev.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reverted the suppression behaviour introduced by TASK-684 (which assumed telnet was always on -- it is not) while keeping the plain-English direction-aware wording on every Unknown-Data-Id frame. The bitmaps stay populated so TASK-686 can consume them.

## Changes
- processOT(): removed the once-per-(id, direction) OTGWDebugTf emit + the suppressTelnetForRepeat flag + the gated OTGWDebugT call.
- Added a short direction-aware suffix on the existing AddLog-built OT-bus log line, emitted on every Unknown-Data-Id frame:
  - read direction:  "(boiler does not implement)"
  - write direction: "(boiler rejected write)"
- lastMasterWasWrite, unknownLoggedRead, unknownLoggedWrite bitmaps preserved; ownership transfers from "drives suppression" to "feeds the support map" for TASK-686.

## Net effect on the next crashevans-style capture
Every Unknown-Data-Id line now carries its own plain-English explanation and remains visible regardless of when the tester connects to telnet. No new lines added, no lines suppressed. ~145 lines / 10 min stays ~145 lines / 10 min, but each one is self-describing.

## Memory and risk
- 0 net new RAM vs TASK-684 (96 bytes static in processOT, repurposed).
- No behaviour change to MQTT publishing, value decoding, gateway substitution, or the WebSocket OT Monitor stream.

## Verification
- python build.py --firmware exits 0 (1.6.0-beta.20+b4ee52c).
- python evaluate.py --quick: 34 passed / 0 warnings / 0 failures (100% health).

## Branch hygiene
- TASK-687 (HA discovery suppression, milestone 2.1.0) was accidentally created on this branch. The creation commit was reverted (a5af0e9b) and re-applied on a fresh feature/milestone-2.1.0 branch so the future-improvement plan lives with its milestone and does not ride PR #640 into dev.
<!-- SECTION:FINAL_SUMMARY:END -->
