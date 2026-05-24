---
id: TASK-685
title: >-
  Revise Unknown-Data-Id logging: drop TASK-684 suppression, keep plain-English
  direction-aware text on every emit
status: In Progress
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
