---
id: TASK-1320
title: >-
  capture-otgw.sh: summary.txt of an earlier session after a telnet drop
  (FileNotFoundError)
status: To Do
assignee: []
created_date: '2026-10-23T13:13:00Z'
updated_date: '2026-10-23T13:19:00Z'
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
- [ ] #1 Reproduction: a fake telnet server closing mid-capture shows the traceback on the current script (red)
- [ ] #2 Fix writes the summary into the current session dir and the run continues/stops cleanly (green); the .bat side checked for the same pattern
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 13:40 diagnosis (read-only): the script passes summary.txt per run (scripts/capture-mqtt-debug.sh:1618 SUMMARY_LOG=$RUN_PATH/summary.txt, given to each Python worker as argv, :753/:1138/:1404). The failing path ./20261023-105403/ belongs to an EARLIER run (10:54) while the current run was ./20261023-144036: background workers of the 10:54 run were still alive (telnet worker: traceback at 'drop'->add_summary; a second worker's 'summary' at its line 19, likely browser/crashlog). Since TASK-1299 the telnet worker reconnects forever instead of exiting on a drop, so an orphan of a run that was not stopped cleanly (terminal closed / killed) keeps running and hits the reboot of the next session; its run dir no longer existed (moved/zipped). Fix direction: workers exit when the parent capture process is gone (pass $$, check os.getppid()/kill -0) or when their run dir/stop file dir disappears, and the parent trap kills its children on EXIT/INT/TERM/HUP. Reproduce: start a run, kill -9 the parent, delete its dir, restart the fake telnet server -> traceback (red).
<!-- SECTION:NOTES:END -->
