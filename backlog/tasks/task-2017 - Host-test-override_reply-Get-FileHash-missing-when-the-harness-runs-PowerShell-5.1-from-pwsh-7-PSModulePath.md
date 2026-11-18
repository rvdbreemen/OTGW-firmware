---
id: TASK-2017
title: >-
  Host test override_reply: Get-FileHash missing when the harness runs
  PowerShell 5.1 from pwsh 7 (PSModulePath)
status: To Do
assignee: []
created_date: '2026-11-18T08:50:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1035000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test/host/test_override_reply.py fails 4/4 in run_tests (seen 2026-11-18 on origin/dev ebdef38fd, identical without any change): build_and_run_override_reply.ps1 reports 'Get-FileHash is not recognized' (CommandNotFoundException). Cause (coordinator): PS 5.1 started from pwsh 7 inherits pwsh's PSModulePath, so Microsoft.PowerShell.Utility is not found. Fix: reset PSModulePath in the launcher, or hash with certutil / python hashlib instead of Get-FileHash. Test infra only, no src.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 test/host/test_override_reply.py is green when run_tests.py is started from pwsh 7 and from Git Bash
- [ ] #2 The failure is reproduced first (red) on the current harness from the same shell
<!-- AC:END -->
