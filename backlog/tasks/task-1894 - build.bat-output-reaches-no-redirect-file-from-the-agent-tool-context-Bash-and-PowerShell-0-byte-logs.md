---
id: TASK-1894
title: >-
  build.bat output reaches no redirect file from the agent tool context (Bash
  and PowerShell, 0-byte logs)
status: To Do
assignee: []
created_date: '2026-11-13T09:30:00Z'
labels:
  - build
  - tooling
dependencies: []
priority: low
ordinal: 964000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-13, TASK-1884 beta.221: three esp32-combo builds of e5112267e exited 0 with fresh artifacts, but none produced a log with the success lines the push check asks for. PowerShell '& build.bat 2>&1 | Select-Object -Last 25' lost its output when the tool moved the job to the background; Git Bash 'cmd.exe //c build.bat > file 2>&1' and PowerShell '& build.bat *> file' gave 0-byte files. build.bat sets PYTHONUNBUFFERED=1 and build.py prints with print(), so buffering is not the cause. Wanted: a reliable build log for agents, e.g. build.py writing its own log file (tee) under build/ or logs/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A documented command gives a build log that contains 'Successfully created ESP32S3 image' (x2) and 'Build completed successfully' when run from Claude's Bash and PowerShell tools
- [ ] #2 docs/guides/agent-build-and-bench.md names that command
<!-- AC:END -->
