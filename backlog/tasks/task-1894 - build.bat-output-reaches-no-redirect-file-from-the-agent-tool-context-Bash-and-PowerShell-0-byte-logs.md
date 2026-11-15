---
id: TASK-1894
title: >-
  build.bat output reaches no redirect file from the agent tool context (Bash
  and PowerShell, 0-byte logs)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T09:30:00Z'
updated_date: '2026-11-15T00:01:00Z'
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
- [x] #1 A documented command gives a build log that contains 'Successfully created ESP32S3 image' (x2) and 'Build completed successfully' when run from Claude's Bash and PowerShell tools
- [x] #2 docs/guides/agent-build-and-bench.md names that command
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: one cause found and split off as TASK-1898: in a fresh worktree build.bat :find_python runs C:\tools\Codex\python.cmd without 'call' and ends silently with exit 1 (echo-on trace). Not every 0-byte log is explained by it: the TASK-1871 build (worktree with .build-venv) built fine but PowerShell 'cmd /c build.bat *> file' still gave 0 bytes, while Start-Process -RedirectStandardOutput/-RedirectStandardError captured the full log of the TASK-1885 build.

2026-11-14 (92, lane wt-92-webui). Cause beyond TASK-1898: run_command() streamed children with stdout=None, so pio/esptool wrote to file descriptor 1 directly, past Python; check_rtc_layout and verify-flash did the same. Fix in build.py: start_build_log() tees sys.stdout/sys.stderr into build/logs/build-<time>.log (path printed at the start), run_streamed() pipes a child's stdout+stderr through Python line by line (run_command's streamed path, check_rtc_layout, verify-flash), stop_build_log() at exit copies the log to build/logs/latest.log. build/ is gitignored. Guide: docs/guides/agent-build-and-bench.md 'Build log (TASK-1894)' names the command and the two grep checks. Test tests/test_build_log_tee.py (real build.py helpers, a child Python process printing the esptool success line to stdout and a warning to stderr): fix 2 passed; old build.py 2 failed. test_build_lock, test_build_jobs, test_build_bat_call_probes OK; test_build_staging 18 tests PASS; evaluate --quick 0 FAIL. Open: AC1 on a real build (the next build of this lane): latest.log must hold 'Successfully created ESP32S3 image' x2 and 'Build completed successfully', run from the Bash and PowerShell tools.

2026-11-15 (92) AC1 shown on a real build: lane wt-92-webui @ 7ecd150d4, esp32-combo 2.0.0-beta.271+7ecd150, started detached from the PowerShell tool (Start-Process cmd.exe /c <full path>\build.bat, stdout/stderr redirected). build/logs/build-20261115-004247.log = build/logs/latest.log, 72,708 B; it starts with 'Build log: ... (copied to build/logs/latest.log at the end)', holds 4 x 'Successfully created ESP32S3 image' (the redirected stdout has the same 4) and 1 x 'Build completed successfully'. The Bash tool reads it with grep (as in the guide). Not shown: a build started from the Bash tool itself; the log file does not depend on the launcher.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: on the real build of lane wt-92-webui (beta.271+7ecd150, started from the PowerShell tool) build/logs/latest.log holds 4x 'Successfully created ESP32S3 image' and 1x 'Build completed successfully', equal to the redirected stdout. AC2: docs/guides/agent-build-and-bench.md 'Build log (TASK-1894)' names the command and the grep checks. Cause: build.py streamed child tools with stdout=None (past Python); now stdout/stderr are teed to build/logs/build-<time>.log and children run through run_streamed(). tests/test_build_log_tee.py red on the old build.py, green on the fix. Not shown: a launch from the Bash tool itself (the log does not depend on the launcher).
<!-- SECTION:FINAL_SUMMARY:END -->
