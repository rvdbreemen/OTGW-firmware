---
id: TASK-2042
title: >-
  Tests that run POSIX scripts pick WSL bash from PowerShell (exit 127, Windows
  path eaten)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T04:56:00Z'
updated_date: '2026-11-19T05:00:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1048000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From PowerShell/cmd, shutil.which('bash') returns C:\Windows\system32\bash.EXE (the WSL launcher). WSL bash eats the backslashes of a Windows script path: '/bin/bash: D:UsersRobert...flash_otgw.sh: No such file or directory', exit 127. Hit tests/test_flash_migrate_settings_compare.py (4/9 red on origin 3a0e2ab35, coordinator), tests/test_discord_post_payload.py (ba's red before the 08:00 beta) and tests/test_discord_post_build.py. Fix: one helper tests/gitbash.py find_bash() (Git for Windows bash, never the WSL launcher) used by the three files, script paths passed as as_posix(). Tests only, no firmware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Red on the old tests with WSL bash first on PATH (exit 127), quoted
- [x] #2 Green on the fix with WSL bash first on PATH and with the normal PATH, and from a PowerShell session
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 red (old tests, PATH=/c/Windows/System32 first, Git Bash): flash_migrate_settings_compare 'AssertionError: 127 != 1 : /bin/bash: D:UsersRobertDocumentsGitHubRvdBwt-410flash_otgw.sh: No such file or directory'; discord_post_build same for scripts/discord-post-build.sh; discord_post_payload '127 != 0 : /bin/bash: C:Users...tmp*.sh'. Same as the coordinator's 4/9 on origin 3a0e2ab35 and ba's discord_post_payload red. AC2 green on the fix: WSL first on PATH 9+10+2 passed; normal PATH 9+10+2 passed; PowerShell session (which bash = C:\Windows\system32\bash.EXE, find_bash = C:\Program Files\Git\bin\bash.exe) 21 passed. Change: tests/gitbash.py find_bash() (Git for Windows bash, never the WSL launcher; climbs from git.exe to the Git root), used by the three tests; script paths as_posix(). Not verified: Linux/macOS (helper falls back to the bash on PATH there, unchanged behaviour).
<!-- SECTION:FINAL_SUMMARY:END -->
