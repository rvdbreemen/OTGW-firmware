---
id: TASK-1756
title: Heap steps down at about +5 and +10 min after boot (to 12664 B), cause unknown
status: To Do
assignee: []
created_date: '2026-11-09T17:03:00Z'
labels:
  - heap
  - bug
  - beta.1
dependencies: []
priority: medium
ordinal: 858000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1528 step b (bench .61, beta.162+5786928, 2026-11-09, 10 reboots, run-20261109). Clean boots stay at 22-33 KB in the boot window (setup-early .. drip), so the old 4.1/16.8 KB boot dips are not reproduced. After the boot window the windowed heap minimum still steps down at about +5 min and +10 min after boot, lowest 12664 B. The cause is unknown: candidates are a timer-driven job that starts later (MQTT discovery/republish, NTP, SAT log, OTA check, web-file cache), not a request from the test (clean boots had no HTTP load besides device/info every 5 s, which itself is a candidate). Related TASK-1528 (Done) and TASK-1675.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on the bench with clean boots: the windowed heap minimum around +5 and +10 min after boot, with timestamps, on the current dev build
- [ ] #2 Name the job or allocation that causes each step, with evidence (telnet [HEAP] lines or an instrumented build), and rule out the device/info poll of the test itself
- [ ] #3 Decide with Robert: bound or fix it, or accept it with the numbers recorded
<!-- AC:END -->
