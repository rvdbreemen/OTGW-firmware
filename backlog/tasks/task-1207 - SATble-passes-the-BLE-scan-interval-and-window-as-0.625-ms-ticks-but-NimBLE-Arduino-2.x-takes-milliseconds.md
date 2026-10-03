---
id: TASK-1207
title: >-
  SATble passes the BLE scan interval and window as 0.625 ms ticks, but
  NimBLE-Arduino 2.x takes milliseconds
status: To Do
assignee: []
created_date: '2026-10-03 17:55'
labels:
  - ble
  - sat
dependencies: []
priority: low
ordinal: 329000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1199 verifier. NimBLEScan.cpp:494 and :502 convert milliseconds to ticks (itvl = intervalMs * 16 / 10). SATble.ino:68, :71-72 and :644-646 pass 160 and 80 described as ticks, so the real scan runs with a 160 ms interval and an 80 ms window instead of the 100 ms and 50 ms the comments intend. The duty cycle is 50 % either way.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The intended interval and window are decided and passed in milliseconds; the comments describe the real unit
- [ ] #2 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
