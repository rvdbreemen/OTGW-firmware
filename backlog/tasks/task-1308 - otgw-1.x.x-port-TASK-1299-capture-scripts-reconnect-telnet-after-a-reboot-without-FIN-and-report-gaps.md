---
id: TASK-1308
title: >-
  otgw-1.x.x: port TASK-1299 - capture scripts reconnect telnet after a reboot
  without FIN and report gaps
status: To Do
assignee: []
created_date: '2026-10-23T07:10:00Z'
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
- [ ] #1 The 1.x capture scripts are checked for the half-open gap with a reproduction on the 1.x bench unit (USB or power reset during a capture), old vs fix
- [ ] #2 If affected: keepalive, the gated watchdog and the gap report are ported and the reproduction shows the reconnect and the gap line
<!-- AC:END -->
