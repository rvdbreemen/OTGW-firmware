---
id: TASK-1208
title: Task-watchdog reset (rst=6) under the 8-worker static-file storm on OTGW32
status: To Do
assignee: []
created_date: '2026-10-04 09:23'
labels:
  - esp32
  - watchdog
  - reliability
dependencies: []
priority: medium
ordinal: 330000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1162 (run 2026-10-04). In the TASK-1162 diagnostic run (T1162diag v2 on alpha.404+b6e900a, refresh_storm.py --workers 2,4,6,8 --seed 1124, 33% client aborts after 1024 body bytes), the second deaf window (t=327-368 s) was a reboot. The banner went from boots=32 to 33 with reset reason 6 (task watchdog). The device reboot_log.txt confirms: '2026-10-04 08:51:57 - reboot cause: Task watchdog (6)'. The CDC stream gapped 5.1 s before the reset.
The same log has two earlier unexplained resets on the same bench, during the TASK-1199 BLE measurements on alpha.405 (no PSRAM, BLE on, REST answering 503 'low heap'): 08:00:22 Task watchdog (6) and 08:11:50 Exception/panic (4).
No crash data was kept: /api/v2/device/crashlog reports available:false. The board has been reflashed since. Which task starved the watchdog is unknown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced: the storm (or the BLE low-heap condition) produces a task-watchdog reset on the bench, with the crash or backtrace captured right after the reset and before any reflash
- [ ] #2 Root cause identified: which task missed its watchdog feed and why
- [ ] #3 Fixed or explicitly deferred with the maintainer, with old-vs-fix evidence for a fix
- [ ] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
