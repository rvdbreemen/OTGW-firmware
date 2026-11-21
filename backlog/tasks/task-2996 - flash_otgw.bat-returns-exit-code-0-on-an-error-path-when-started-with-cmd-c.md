---
id: TASK-2996
title: flash_otgw.bat returns exit code 0 on an error path when started with cmd /c
status: To Do
assignee: []
created_date: '2026-11-21T06:22:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1122000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-2965 (2026-11-21, otgw-docs). Repro on Windows: copy flash_otgw.bat into an empty folder with an empty tools\esptool\esptool.exe (no image next to it) and run cmd.exe /c ""<folder>\flash_otgw.bat"" (the PowerShell form) or cmd.exe /c <folder>\flash_otgw.bat: the tool prints '[ERROR] No merged-full bin found.' and cmd returns exit code 0. The old code (before TASK-2965) does the same. Started with call (as the TASK-2965 double-click child does) the same run returns 1. Likely cause, not verified: the exit /b 1 sits inside a parenthesised if/else block in the main flow (Step 2: locate image(s)), and cmd /c reports the last command's level instead. Effect: a script or CI step that runs flash_otgw.bat through cmd /c or PowerShell cannot see a failed flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test runs the real flash_otgw.bat with cmd /c on the no-image error path: red (exit 0) on the current code, green (exit 1) on the fix
- [ ] #2 The double-click wait of TASK-2965 and tests/test_bat_doubleclick_pause.py still pass
<!-- AC:END -->
