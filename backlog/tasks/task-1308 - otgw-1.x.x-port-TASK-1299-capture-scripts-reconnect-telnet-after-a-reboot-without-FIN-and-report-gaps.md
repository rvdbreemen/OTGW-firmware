---
id: TASK-1308
title: >-
  otgw-1.x.x: port TASK-1299 - capture scripts reconnect telnet after a reboot
  without FIN and report gaps
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T07:10:00Z'
updated_date: '2026-10-24T18:51:00Z'
labels:
  - scripts
  - capture
  - 1.x
dependencies: []
priority: low
ordinal: 431000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of TASK-1299 (2.0.0 dev). The otgw-1.x.x line carries its own, different capture scripts (scripts/capture-mqtt-debug.bat and scripts/capture-otgw.sh, checked 2026-10-23 with git ls-tree origin/otgw-1.x.x). TASK-1299 found that a rebooting ESP sends no FIN, so a capture worker that only reconnects on FIN/exception/restart marker stops logging telnet forever (reproduced on the 2.0.0 bench with a USB hard reset: old scripts silent for ~5 min, fixed scripts reconnect in 12-16 s). Check whether the 1.x scripts have the same gap and, if so, port the fix: TCP keepalive (idle 10 s / interval 5 s / 3 probes), the health-probe-gated silence watchdog, and the telnet gap lines in summary.txt. Work in an otgw-1.x.x worktree, not in the dev tree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 1.x capture scripts are checked for the half-open gap with a reproduction on the 1.x bench unit (USB or power reset during a capture), old vs fix
- [x] #2 If affected: keepalive, the gated watchdog and the gap report are ported and the reproduction shows the reconnect and the gap line
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 (from TASK-1320 on dev): the 2.0.0 capture script got an orphan-worker fix: background Python workers exit when the parent script is gone (posix pid / Windows pid from /proc/$$/winpid) or their run dir disappears, ignore SIGHUP so the parent's HUP trap can clean up, and add_summary exits quietly on a missing dir. When porting TASK-1299's reconnect loop to 1.x, port this fix with it; test tests/test_capture_orphan_worker.py.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
1.x capture scripts were partly affected: the .bat had an HTTP-gated 120 s idle watchdog but no keepalive and no gap report. Ported keepalive + SelectError poll + gap report to capture-mqtt-debug.bat and the gap report to capture-otgw.sh (bash /dev/tcp cannot set keepalive). Commit dca99273f on origin/otgw-1.x.x. Evidence (1.x bench .88.68, ESP-only RTS reset during capture): old .bat telnet silent 20:37:13 -> 20:39:13 (120 s, idle watchdog, no gap line); new .bat reconnect after 11 s via keepalive, 'Telnet gap: ... (11 s, 0 failed connect attempts, connection closed)', 'Telnet gaps: 1, total 11 s'; .sh run produced its gap line (44 s, watchdog 30 s). Not verified: .sh last tweak only bash -n; .sh not run on Linux/macOS.
<!-- SECTION:FINAL_SUMMARY:END -->
