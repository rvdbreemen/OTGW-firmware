---
id: TASK-1564
title: >-
  Field panic ipc1 (Sergeant D, alpha.566): root cause from the next full field
  dump
status: To Do
assignee: []
created_date: '2026-11-02T18:11:00Z'
updated_date: '2026-11-02T18:12:00Z'
labels:
  - crash
  - field
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 715000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1538 AC#1 (Robert, 2026-11-02). Since 2.0.0-beta.10 (TASK-1538) the crash-log summary keeps all 16 backtrace frames. When the ipc1 Exception/panic inside heap_caps_malloc recurs on a field unit, collect /api/v2/device/crashlog (and the full core dump over USB: esptool read_flash 0x3F0000 0x10000 + esp-coredump with the matching .elf), name the application caller with file:line, and fix it with an old-vs-fix reproduction.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A field crash log or core dump of the ipc1 panic on 2.0.0-beta.10 or later is obtained and the application caller is named with file:line
- [ ] #2 Cause fixed with an old-vs-fix reproduction, or documented as not reproducible with the evidence
<!-- AC:END -->
