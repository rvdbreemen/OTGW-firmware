---
id: TASK-1438
title: >-
  Bench Classic+S3: three reboots 'reboot cause: Unknown (b)' within 90 s on
  alpha.519 during a replay run
status: To Do
assignee: []
created_date: '2026-10-28T22:18:00Z'
labels:
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 599000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-10-28 21:20:27, 21:21:31, 21:21:56 on the Classic+S3 (.63), alpha.519+49135fe, SAT on, OT replay running (TASK-1420 bench run, S4 first attempt). /reboot_log.txt: 'reboot cause: Unknown (b)', no crashlog. No USB, no PIC actions. Cause code 0xb not mapped yet. Logs: %TEMP%\otgw-bench-1420\A_519\S4_try2_reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reset reason 0xb mapped against the ESP-IDF reset reasons and the firmware's reboot log code; cause explained or reproduced
- [ ] #2 If a firmware cause: fix with red/green evidence; else documented as external
<!-- AC:END -->
