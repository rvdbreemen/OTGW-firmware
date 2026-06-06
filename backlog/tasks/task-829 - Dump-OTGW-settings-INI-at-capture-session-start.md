---
id: TASK-829
title: Dump OTGW settings (INI) at capture session start
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 06:02'
updated_date: '2026-06-06 06:03'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After enabling debug toggles, capture-mqtt-debug.bat should send the 'D' command (full INI dump, per telnet menu footer) so the captured telnet.log records the device settings at the start of each session. Helps diagnose config-specific issues (e.g. TASK-779 heap/WS) without asking the tester to dump manually.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On telnet connect (after toggle-enable), 'D' is sent and the full INI dump is drained into telnet.log
- [x] #2 Dump drain ends on idle (no fixed long sleep) with a hard timeout cap
- [x] #3 Runs on every (re)connect so a post-reboot settings snapshot is re-captured
- [x] #4 summary.txt records the dump action
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add Request-SettingsDump (send 'D', drain output to telnet.log until idle/timeout)\n2. Call it in Connect-TelnetCapture after Enable-AllTelnetDebugIfNeeded; add summary line\n3. Parse-check + commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a settings (INI) dump to capture-mqtt-debug.bat at session start.

New Request-SettingsDump function sends the telnet 'D' command (full INI dump, per the menu footer) right after the debug toggles are enabled in Connect-TelnetCapture, draining the multi-line output into telnet.log. Drain ends on an 800ms idle window with a 5s hard timeout cap (no fixed long sleep). Runs on every (re)connect so a post-reboot settings snapshot is re-captured. summary.txt logs the action; Show-Help updated.

Verification: PowerShell AST parse clean (1049 lines, 0 errors); call order confirmed (toggle-enable precedes settings-dump). Script-only change (no firmware); build/eval gates N/A.
<!-- SECTION:FINAL_SUMMARY:END -->
