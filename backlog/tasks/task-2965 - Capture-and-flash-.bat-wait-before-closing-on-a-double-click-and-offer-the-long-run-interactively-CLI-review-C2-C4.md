---
id: TASK-2965
title: >-
  Capture and flash .bat: wait before closing on a double-click, and offer the
  long run interactively (CLI review C2 C4)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:29:00Z'
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
- [ ] #1 Started by double-click, both .bat tools end with 'Press any key to close this window.' on success and on every error path; started from cmd or PowerShell they exit without waiting
- [ ] #2 The interactive capture asks 'How long? 1) Until you press Q 2) 24 hours (long run, recommended for SAT field data) 3) 3 days'; choice 2 gives the same run as --long-run --duration 86400; the answer is saved as the next default
- [ ] #3 --no-prompt and explicit --duration/--long-run skip the question; the existing tests for otgw_capture.py pass and a new test covers the duration question
<!-- AC:END -->
