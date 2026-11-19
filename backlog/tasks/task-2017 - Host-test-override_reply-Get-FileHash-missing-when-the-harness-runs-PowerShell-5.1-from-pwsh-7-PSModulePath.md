---
id: TASK-2017
title: >-
  Host test override_reply: Get-FileHash missing when the harness runs
  PowerShell 5.1 from pwsh 7 (PSModulePath)
status: Done
assignee:
  - '@otgw-dev'
created_date: '2026-11-18T08:50:00Z'
updated_date: '2026-11-18T09:01:00Z'
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
- [x] #1 test/host/test_override_reply.py is green when run_tests.py is started from pwsh 7 and from Git Bash
- [x] #2 The failure is reproduced first (red) on the current harness from the same shell
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Cause, made exact (otgw-dev, 2026-11-18): the 5.1 module is not missing. The inherited PSModulePath does hold C:\WINDOWS\system32\WindowsPowerShell\v1.0\Modules, but after c:\program files\powershell\7\Modules. Get-Module -ListAvailable Microsoft.PowerShell.Utility in 5.1 lists 7.0.0.0 (pwsh 7 path) first, then 3.1.0.0; command autoload picks the 7.0 copy, 5.1 cannot load it, so Get-FileHash is not recognized. Probe: with pwsh's paths dropped from PSModulePath (filter '\PowerShell\', which does not match '\WindowsPowerShell\'), Get-FileHash works in the same 5.1 process.

Fix: build_and_run_override_reply.ps1 and build_and_run_loopback.ps1 (same Get-FileHash call) drop PowerShell 7's module paths from PSModulePath right after param(), only under Windows PowerShell (PSEdition Desktop). In the script, not the Python launcher, so the manual old-vs-fix runs from pwsh are covered too.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 + AC2, same tree (lane-ovr on 460199594), scripts reverted to HEAD for the red run. Git Bash: OLD 4/4 fail ('Get-FileHash' is not recognized, CommandNotFoundException), FIX Ran 4 tests OK (67.6 s). pwsh 7.6.9: OLD Ran 4 FAILED (failures=4), FIX Ran 4 tests OK (65.3 s). Also build_and_run_loopback.ps1 from pwsh with the fix: RESULT: PASS. Not shown: a red run of the loopback script (no test wrapper; only the same call and the same fix). Test infra only, no src, no bump.
<!-- SECTION:FINAL_SUMMARY:END -->
