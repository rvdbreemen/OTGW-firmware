---
id: TASK-1955
title: 'esp32-combo app slot at 93.8 %: 141 KB headroom left in ota_0 (beta.287)'
status: To Do
assignee: []
created_date: '2026-11-15T03:20:00Z'
labels:
  - build
  - flash-size
dependencies: []
priority: low
ordinal: 997000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Measured on beta.287+9d1510b (build log, 2026-11-15): Flash 2152623 of 2293760 bytes (93.8 %), 141137 B free in the single app slot ota_0 (0x100000, 2240K, ADR-191 safeboot layout). TASK-1639 (GitHub #694) did the size breakdown at 93.5 % (beta.89) and Robert chose only the siscanf reduction then. OTA of 287 on .61 worked (09, changed: none). Safeboot updater writes ota_0 from the factory partition, so the limit is the ota_0 size, not twice the image. Growth since beta.89: +0.3 points. Purpose: keep the headroom visible per beta so a feature that needs more than the remaining ~141 KB is planned (reduction candidates of TASK-1639) instead of found at build time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A table of app size per beta from the build logs since beta.89 (at least 5 points) with the free bytes in ota_0
- [ ] #2 A warning threshold proposed to Robert (e.g. build.py WARN above 95 %), with the consequence of each option
<!-- AC:END -->
