---
id: TASK-2965
title: >-
  Capture and flash .bat: wait before closing on a double-click, and offer the
  long run interactively (CLI review C2 C4)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T15:29:00Z'
updated_date: '2026-11-21T06:21:00Z'
labels:
  - tools
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1097000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2963 CLI review (docs/reviews/2026-11-20_ux-cli-tools/README.md, C2 and C4). C2: neither .bat pauses at the end (capture-mqtt-debug.bat:38-40, flash_otgw.bat exits on every path, no %cmdcmdline% check), so on a double-click the window closes with the 'Upload this one file' line (otgw_capture.py:4163-4176) or any error message. C4: the interactive capture flow asks only device, broker and user (otgw_capture.py:3731, 3769, 3775), never the duration, so a double-clicking tester cannot start the long run the field-run recipe asks for (--long-run, otgw_capture.py:3559-3562).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Started by double-click, both .bat tools end with 'Press any key to close this window.' on success and on every error path; started from cmd or PowerShell they exit without waiting
- [x] #2 The interactive capture asks 'How long? 1) Until you press Q 2) 24 hours (long run, recommended for SAT field data) 3) 3 days'; choice 2 gives the same run as --long-run --duration 86400; the answer is saved as the next default
- [x] #3 --no-prompt and explicit --duration/--long-run skip the question; the existing tests for otgw_capture.py pass and a new test covers the duration question
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: taken over by otgw-docs (95) on the coordinator's assignment (scripts only, dry run on Windows, no unit).

2026-11-21 otgw-docs (95). C2: both .bat files recognise the Explorer start (cmdcmdline ends in " ": the batfile open command is "%1" %*, so cmd runs /c ""<bat>" "; PowerShell gives /c ""<bat>"" and cmd gives /c <bat>, probed on this PC), then run themselves once more as a child (call "%~f0", guard OTGW_PAUSE_CHILD) and end with 'Press any key to close this window.' keeping the exit code. No restructuring of the existing exit /b paths, so every error path is covered.

C4: otgw_capture.py ask_duration() asks 'How long? 1) Until you press Q 2) 24 hours (long run, recommended for SAT field data) 3) 3 days' at the end of resolve_inputs() when a console is present and neither --duration nor --long-run nor --no-prompt is given; 2 = long_run True + duration 86400 (same as --long-run --duration 86400), 3 = 259200; saved as DurationChoice and offered as the next default; 3 wrong answers take the default (found when test_capture_discovery hung: its input stub always answers an IP). Docs: scripts/README.md usage, field-run recipe step 3.

Evidence (Windows, no unit). New tests, red on HEAD 06850068f-line code in a temp worktree: test_capture_duration_prompt 3 FAIL + 1 ERROR of 6 (e, f are guards, green on both); test_bat_doubleclick_pause 3 FAIL of 4 (d guard). On the fix: duration 6/6 OK, bat 4/4 OK. Existing: test_capture_tool 63 OK (2 skipped), test_capture_discovery 34 OK (test_g fallback got --duration 60: it pins the device prompt), test_capture_launcher_bat 6, test_capture_launcher 7, py39_compat 3, mqtt_own_topics 8, device_data 21, flash_zip_capture_imports 3, all OK. evaluate --quick: Failed 0. Pre-existing, not changed: flash_otgw.bat returns exit code 0 on '[ERROR] No merged-full bin found.' when started with cmd /c (old and new code alike); via call it returns 1. Not verified: a real Explorer double-click by hand (the test reproduces Explorer's exact command line).

Correction: the red run used a temp worktree at d61678eef (origin/dev HEAD then), not 06850068f.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Double-click on capture-mqtt-debug.bat / flash_otgw.bat now ends with 'Press any key to close this window.' (success and every error path, exit code kept); from cmd or PowerShell nothing waits. The interactive capture asks the duration (1 until Q, 2 = --long-run --duration 86400, 3 = 3 days), saved as the next default, skipped by --no-prompt/--duration/--long-run. AC1: tests/test_bat_doubleclick_pause.py runs the real .bat copies with Explorer's exact command line: red 3/4 on the old code, 4/4 green. AC2+AC3: tests/test_capture_duration_prompt.py on the real resolve_inputs/save_settings: red on the old code, 6/6 green; existing capture tests all green (test_capture_tool 63, discovery 34, launcher_bat 6, launcher 7, py39 3, own_topics 8, device_data 21, zip imports 3). evaluate --quick Failed 0. Not verified: a manual double-click in Explorer. Pre-existing flash_otgw.bat exit code 0 under cmd /c on its error path noted, not changed.
<!-- SECTION:FINAL_SUMMARY:END -->
