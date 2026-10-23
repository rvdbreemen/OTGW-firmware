---
id: TASK-1320
title: >-
  capture-otgw.sh: summary.txt of an earlier session after a telnet drop
  (FileNotFoundError)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T13:13:00Z'
updated_date: '2026-10-23T14:05:00Z'
labels:
  - tooling
  - capture
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 443000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 12:51Z (#alpha-testing): the Linux capture logger from alpha.465, run across a firmware upgrade, printed 'ConnectionError: remote closed connection' then 'FileNotFoundError: ./20261023-105403/summary.txt' while the session dir was ./20261023-144036. The reconnect/summary code (TASK-1299) writes to a stale session directory (from a previous run). Reproduce with a fake telnet server that closes the connection.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduction: a fake telnet server closing mid-capture shows the traceback on the current script (red)
- [x] #2 Fix writes the summary into the current session dir and the run continues/stops cleanly (green); the .bat side checked for the same pattern
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Bug: orphaned Python workers of an earlier capture run (parent killed / terminal closed) keep reconnecting since TASK-1299 and write to their old, deleted run dir (FileNotFoundError ./20261023-105403/summary.txt during the 14:40 run). Fix in scripts/capture-mqtt-debug.sh (and capture-mqtt-debug-macos.sh if it has the same workers): (1) pass the parent PID to every background Python worker (telnet, browser, crashlog, probes) and have each loop exit quietly when the parent is gone (os.kill(ppid,0) fails / os.getppid() changed) or when its run dir no longer exists; (2) add_summary/summary writes tolerate a missing dir (exit instead of traceback); (3) the parent trap kills its children on EXIT INT TERM HUP. Check the .bat (Windows) for the same orphan pattern and note the result. Reproduction harness (tests/test_capture_orphan_worker.py or a bash test): fake telnet server on localhost, start the real telnet worker code from the script, kill the parent / delete the run dir, close the server connection -> HEAD: traceback + keeps running (red); fix: exits within a bounded time, no traceback (green). Cross-line: 1.x has its own capture copy -> TASK-1308 already tracks the 1.x port of TASK-1299; add a note there.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 13:40 diagnosis (read-only): the script passes summary.txt per run (scripts/capture-mqtt-debug.sh:1618 SUMMARY_LOG=$RUN_PATH/summary.txt, given to each Python worker as argv, :753/:1138/:1404). The failing path ./20261023-105403/ belongs to an EARLIER run (10:54) while the current run was ./20261023-144036: background workers of the 10:54 run were still alive (telnet worker: traceback at 'drop'->add_summary; a second worker's 'summary' at its line 19, likely browser/crashlog). Since TASK-1299 the telnet worker reconnects forever instead of exiting on a drop, so an orphan of a run that was not stopped cleanly (terminal closed / killed) keeps running and hits the reboot of the next session; its run dir no longer existed (moved/zipped). Fix direction: workers exit when the parent capture process is gone (pass $$, check os.getppid()/kill -0) or when their run dir/stop file dir disappears, and the parent trap kills its children on EXIT/INT/TERM/HUP. Reproduce: start a run, kill -9 the parent, delete its dir, restart the fake telnet server -> traceback (red).

2026-10-23 16:20 evidence: tests/test_capture_orphan_worker.py runs the REAL telnet worker heredoc against a fake telnet server. HEAD script: Windows 'FAILED (failures=1)' (dead parent: still running after 15 s); Linux/WSL 'FAILED (failures=2)' incl. the tester's exact traceback 'FileNotFoundError: .../20261023-105403/summary.txt'. Fix: Windows 'OK (skipped=1)' (deleted-dir case skipped: Windows cannot delete a dir with an open log), Linux/WSL 'OK' (3/3). tests/test_capture_telnet_worker.py 'OK (skipped=1)'. bash -n OK. Own review find: $$ under Git Bash is an MSYS PID that native Windows Python cannot see -> parent check would have killed healthy workers on Git Bash; workers now get '<posix>:<winpid>' and pick by os.name (verified: picked winpid alive True; MSYS pid alive False). Reviewer fixes: getppid()==1 shortcut dropped (kill(pid,0) covers it), MSYS-Python safe PID choice, SIGHUP ignored in workers so the parent's HUP trap stops them via the stop file (browser finally closes Edge). .bat: not affected (in-process runspaces). Not verified: a real end-to-end capture session, the HUP path in a real terminal close, browser/crashlog workers beyond compile + reading. Known limit: PID reuse after the parent exits keeps a worker alive (low probability).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Background workers of the Linux/macOS capture script (telnet, browser, crashlog) now stop when their capture run is gone: the parent process ended (POSIX pid, or the real Windows pid under Git Bash) or the run folder was removed. A missing folder no longer raises a traceback; workers ignore SIGHUP so the HUP trap added to the parent can stop them cleanly. Evidence: AC1 the new test reproduces the tester's FileNotFoundError on the old script (Linux) and the never-ending orphan (Windows); AC2 green on the fix on Windows and Linux, existing worker test green; the .bat was checked and is not affected. Not verified: a real end-to-end session and a real terminal close.
<!-- SECTION:FINAL_SUMMARY:END -->
