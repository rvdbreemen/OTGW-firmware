---
id: TASK-1560
title: >-
  Crash diagnostics: offer the stored ESP32 core dump for download (REST), so a
  field panic can be decoded offline
status: To Do
assignee: []
created_date: '2026-11-02T12:52:00Z'
labels:
  - crash
  - diagnostics
dependencies: []
priority: medium
ordinal: 712000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1538. The reboot log now keeps all 16 backtrace frames of the core-dump summary, but the application caller can still be deeper (the ipc1 panic of alpha.566 entered through _frxt_int_enter inside heap_caps_malloc). The full ELF core dump stays in the coredump partition (0x3F0000, 64 KB, partitions_otgw_esp32.csv) and today can only be read over USB (esptool read_flash 0x3F0000 0x10000, then esp-coredump with the matching .elf). A GET endpoint that streams the partition would let a field tester send it without USB. New REST contract: needs an ADR (admin-protected? size, streaming, ADR-056/146), docs/api update.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR (Proposed) for the endpoint: auth, streaming, response format
- [ ] #2 Endpoint streams the coredump partition; bench: download decodes with esp-coredump against the matching .elf
- [ ] #3 docs/api and the capture tool mention it
<!-- AC:END -->
